#include <types.h>
#include <stdlib.h>
#include "q40uart.h"
#include "timers.h"
#include "ff.h"
#include "tinyalloc.h"
#include "cli.h"
#include "net.h"

// documentation:
// https://www.rfc-editor.org/rfc/rfc1350 - TFTP Protocol (Revision 2)
// https://www.rfc-editor.org/rfc/rfc2347 - TFTP Option Extension
// https://www.rfc-editor.org/rfc/rfc2349 - TFTP Timeout Interval and Transfer Size Options
// https://www.rfc-editor.org/rfc/rfc7440 - TFTP Windowsize Option
// https://www.compuphase.com/tftp.htm - Extending TFTP

/*
 * TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO 
 *
 * fix ne2000 "too big" issue
 * (refer to linux driver receive code?)
 * it would make sense to send an ACK and *then* write data to disk
 * then we overlap the write op with receiving more data
 *
 * TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO 
 */

#define RRQ_TIMEOUT 1000 // ms
#define DATA_TIMEOUT 250 // ms

typedef struct tftp_transfer_t tftp_transfer_t;

struct tftp_transfer_t {
    char *tftp_filename;
    char *disk_filename;
    FIL disk_file;
    uint16_t block_size;
    uint16_t last_block;
    uint16_t last_ack;
    uint16_t rollover_value;
    int bytes_received;
    int total_size;
    int window_size;
    bool started;
    bool completed;
    bool success;
    int retransmits_this_block;
    int retransmits_total;
};

typedef struct tftp_header_t tftp_header_t;

struct __attribute__((packed, aligned(2))) tftp_header_t {
    uint16_t opcode;
    union payload {
        uint8_t raw[0];
        struct {
            uint16_t block_number;
            uint8_t data[];
        } data;
        struct {
            uint16_t block_number;
        } ack;
        struct {
            uint16_t error_code;
            uint8_t error_message[];
        } error;
    } payload;
};

static const uint16_t tftp_op_rrq = 1;
//static const uint16_t tftp_op_wrq = 2;
static const uint16_t tftp_op_data = 3;
static const uint16_t tftp_op_ack = 4;
static const uint16_t tftp_op_err = 5;
static const uint16_t tftp_op_options_ack = 6;

#define MAXOPT 1400
static int options_append(char *options, int offset, char *extra)
{
    int extra_len = strlen(extra) + 1;

    if(extra_len+offset >= MAXOPT){
        printf("tftp: options too long!");
        return 0;
    }

    memcpy(options+offset, extra, extra_len);
    return offset + extra_len;
}

static packet_t *tftp_create_rrq(packet_sink_t *sink)
{
    tftp_transfer_t *tftp = sink->sink_private;
    char options[MAXOPT];
    int offset = 0;

    offset = options_append(options, offset, tftp->tftp_filename);
    offset = options_append(options, offset, "octet");

    offset = options_append(options, offset, "rollover");
    offset = options_append(options, offset, "0");

    offset = options_append(options, offset, "tsize");
    offset = options_append(options, offset, "0");

    offset = options_append(options, offset, "blksize");
    offset = options_append(options, offset, "1024");

    offset = options_append(options, offset, "windowsize");
    offset = options_append(options, offset, "8"); // could maybe do 8?

    packet_t *packet = packet_create_for_sink(sink, offset + 2);
    packet->udp->destination_port = htons(69); // RRQ always goes to port 69
    tftp_header_t *message = (tftp_header_t*)packet->data;
    message->opcode = htons(tftp_op_rrq);
    memcpy(message->payload.raw, options, offset);

    return packet;
}

static packet_t *tftp_create_ack(packet_sink_t *sink)
{
    tftp_transfer_t *tftp = sink->sink_private;

    packet_t *packet = packet_create_for_sink(sink, 4);
    tftp_header_t *message = (tftp_header_t*)packet->data;

    message->opcode = htons(tftp_op_ack);
    message->payload.ack.block_number = htons(tftp->last_block);
    tftp->last_ack = tftp->last_block;

    return packet;
}

static void tftp_timer_expired(packet_sink_t *sink)
{
    tftp_transfer_t *tftp = sink->sink_private;
    
    printf("tftp_timer_expired\n");

    if(tftp->retransmits_this_block > 10){
        tftp->completed = true;
        tftp->success = false;
        return;
    }

    if(tftp->completed)
        return;

    if(!tftp->started){
        sink->timer = set_timer_ms(RRQ_TIMEOUT);
        net_tx(tftp_create_rrq(sink));
    }else{
        sink->timer = set_timer_ms(DATA_TIMEOUT);
        net_tx(tftp_create_ack(sink));
    }

    tftp->retransmits_this_block++;
    tftp->retransmits_total++;
}

static void tftp_process_options_ack(packet_sink_t *sink, tftp_header_t *message, int message_len)
{
    tftp_transfer_t *tftp = sink->sink_private;
    char *opt, *val, *ptr, *end;
    int val_int;

    ptr = (char*)message->payload.raw;
    end = ptr+message_len-2;

    printf("tftp: options ack:");

    while(true){
        // get option
        opt = ptr;
        while(*ptr && ptr < end)
            ptr++;
        if(ptr >= end)
            break;
        ptr++;
        // get value
        val = ptr;
        while(*ptr && ptr < end)
            ptr++;
        if(ptr >= end)
            break;
        ptr++;
        // process option + value
        val_int = atoi(val);
        if(!strcmp(opt, "rollover")){
            tftp->rollover_value = val_int;
        }else if(!strcmp(opt, "tsize"))
            tftp->total_size = val_int;
        else if(!strcmp(opt, "blksize"))
            tftp->block_size = val_int;
        else if(!strcmp(opt, "windowsize"))
            tftp->window_size = val_int;
        printf(" %s=%d", opt, val_int);
    }

    printf("\n");

    // send an ACK with block=0 to agree to the options
    tftp->last_block = 0;
    net_tx(tftp_create_ack(sink));
    sink->timer = set_timer_ms(DATA_TIMEOUT);
}

static void tftp_process_data(packet_sink_t *sink, tftp_header_t *message, int message_len)
{
    tftp_transfer_t *tftp = sink->sink_private;
    int size;
    uint16_t rxblock, expected_block;

    rxblock = ntohs(message->payload.data.block_number);
    expected_block = tftp->last_block + 1;

    if(expected_block == 0)
        expected_block = tftp->rollover_value;

    if(rxblock == expected_block){ // is it the block we are expecting?
        tftp->last_block = rxblock;
        tftp->retransmits_this_block = 0;
        size = message_len - 4;
        if(size > 0){
            FRESULT fr;
            // can we defer the f_write until we send an ACK?
            // then we can overlap it with waiting for the next blocks
            fr = f_write(&tftp->disk_file, message->payload.data.data, size, NULL);
            tftp->bytes_received += size;
            if(fr != FR_OK){
                printf("tftp: failed to write to \"%s\": %s\n", tftp->disk_filename, f_errmsg(fr));
                tftp->completed = true;
                tftp->success = false;
            }
        }
        if(!tftp->completed && size < tftp->block_size){
            // a short data block indicates success
            tftp->completed = true;
            tftp->success = true;
        }
    }

    if(tftp->last_block >= (tftp->last_ack + tftp->window_size))
        net_tx(tftp_create_ack(sink));
    sink->timer = set_timer_ms(DATA_TIMEOUT);
}

static void tftp_packet_received(packet_sink_t *sink, packet_t *packet)
{
    tftp_transfer_t *tftp = sink->sink_private;
    tftp_header_t *message = (tftp_header_t*)packet->data;

    if(!tftp->started){
        // lock on to the server's source port
        net_remove_packet_sink(sink);
        sink->match_remote_port = ntohs(packet->udp->source_port);
        net_add_packet_sink(sink);
        printf("tftp: server using port %d\n", sink->match_remote_port);
        tftp->started = true;
    }

    switch(ntohs(message->opcode)){
        case tftp_op_options_ack:
            tftp_process_options_ack(sink, message, packet->data_length);
            break;
        case tftp_op_data:
            tftp_process_data(sink, message, packet->data_length);
            break;
        case tftp_op_err:
            printf("tftp: server error code 0x%0x: %s\n",
                    ntohs(message->payload.error.error_code), message->payload.error.error_message);
            tftp->completed = true;
            tftp->success = false;
            break;
        default:
            printf("tftp: unexpected server response, opcode 0x%04x\n", ntohs(message->opcode));
            pretty_dump_memory(packet->buffer, packet->buffer_length);
            tftp->completed = true;
            tftp->success = false;
            break;
    }

    packet_free(packet);
}

bool tftp_receive(uint32_t tftp_server_ip, const char *tftp_filename, const char *disk_filename)
{
    uint32_t start, taken, rate;
    int uart_byte, reported_received;
    packet_sink_t *sink = packet_sink_alloc();
    tftp_transfer_t *tftp = malloc(sizeof(tftp_transfer_t));
    memset(tftp, 0, sizeof(tftp_transfer_t));

    sink->match_interface_local_ip = true;
    sink->match_ipv4_protocol = ip_proto_udp;
    sink->match_remote_ip = tftp_server_ip;
    sink->match_local_port = 8192 + (q40_read_timer_ticks() & 0x7fff);
    sink->sink_private = tftp;
    tftp->block_size = 512;
    tftp->window_size = 1;
    tftp->tftp_filename = strdup(tftp_filename);
    tftp->disk_filename = strdup(disk_filename);

    FRESULT fr = f_open(&tftp->disk_file, tftp->disk_filename, FA_WRITE | FA_CREATE_ALWAYS);

    if(fr != FR_OK){
        printf("tftp: failed to open \"%s\": %s\n", tftp->disk_filename, f_errmsg(fr));
    }else{
        start = q40_read_timer_ticks();
        sink->cb_packet_received = tftp_packet_received;
        sink->cb_timer_expired = tftp_timer_expired;
        net_add_packet_sink(sink);
        tftp_timer_expired(sink); // synthesise a timeout; triggers transmission of RRQ
        tftp->retransmits_total = tftp->retransmits_this_block = 0; // fixup count

        printf("Transfer started: Press Q to abort\n");

        reported_received = 0;
        while(!tftp->completed){
            net_pump(); // this calls our callsbacks to make the transfer go
            uart_byte = uart_read_byte();
            if(uart_byte == 'q' || uart_byte == 'Q'){
                printf("Aborted.\n");
                break;
            }
            if((tftp->bytes_received - reported_received) >= (256*1024) || 
               (tftp->total_size && tftp->bytes_received == tftp->total_size)){
                reported_received = tftp->bytes_received;
                printf("tftp: received %d/%d KB\n", reported_received >> 10, tftp->total_size >> 10);
            }
        }

        if(tftp->success){
            printf("Transfer success.\n");
            taken = q40_read_timer_ticks() - start;
            taken /= (TIMER_HZ/10); // taken is now in 10ths of a second
            if(taken == 0)
                taken = 1; // avoid div 0
            rate = ((tftp->bytes_received / taken)*100) >> 10;
            printf("Transferred %d bytes in %ld.%lds (%ld.%ld KB/sec)\n",
                    tftp->bytes_received, taken/10, taken%10, rate/10, rate%10);
        }else{
            printf("Transfer FAILED!\n");
        }

        // close the file
        f_close(&tftp->disk_file);

        // unregister the sink
        net_remove_packet_sink(sink);
    }

    packet_sink_free(sink);
    free(tftp->tftp_filename);
    free(tftp->disk_filename);
    free(tftp);

    return true;
}