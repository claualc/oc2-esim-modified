#include "bs_connector.hpp"
#include "encode_e2apv1.hpp"
#include "encode_kpm.hpp"
#include "kpm_callbacks.hpp"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <boost/asio.hpp>
extern "C"
{
#include "csv_reader.h"
#include "E2SM-KPM-IndicationMessage.h"
#include "E2SM-KPM-RANfunction-Description.h"
#include "E2SM-KPM-IndicationHeader-Format1.h"
#include "E2SM-KPM-IndicationHeader.h"
#include "Timestamp.h"
#include "E2AP-PDU.h"
}

int report_data_nrt_ric = 1;
uint8_t *indication_request_buffer1;
uint8_t *indication_request_buffer3;
uint8_t *indication_request_buffer2;
int indication_request_length;
bool report_listener_running = false;

// handle timer got from RIC Subscription Request
// timer is in seconds
void handleTimer(E2Sim *e2sim, int *timer, long *ric_req_id, long *ric_instance_id,
                 long *ran_function_id, long *action_id, uint8_t *indreq_buff, int indreq_buflen)
{

  int seq_num = 1;

  fprintf(stderr, "Handle timer %d seconds, ricReqId %ld\n", timer[0], ric_req_id[0]);

  // // populate thread arguments
  // thread_args *t_args = (thread_args*) calloc(1, sizeof(thread_args));
  // t_args->timer = timer;
  // t_args->ric_req_id = ric_req_id;

  // start thread
  report_data_nrt_ric = 1;
  fprintf(stderr, "handletimer called with buff size %d\n", indreq_buflen);
  /*
  fprintf(stderr,"about to print buffer in handle_timer\n");
  for(int i=0; i<indreq_buflen; i++){
      fprintf(stderr,"---%hhx\n",indreq_buff[i]);
  }
  fprintf(stderr,"\n");
  */


  fprintf(stderr, "ACTION TYPE %li\n", *action_id);
  if (*action_id == 1) {


    // saving received buffer in global variable because then a thread will use it (safely, since it is read only)
    // if we pass it directly to the thread, since the scope is local to this function, the behaviour will be undeifined
    indication_request_buffer1 = (uint8_t *)calloc(sizeof(uint8_t), indreq_buflen);
    indication_request_length = indreq_buflen;
    memcpy(indication_request_buffer1, indreq_buff, indreq_buflen);
    
    fprintf(stderr, "RIC REPORT service every %i seconds\n", timer[0]);
    std::thread t1(periodicDataReport, e2sim, timer, seq_num, ric_req_id, ric_instance_id,
              ran_function_id, action_id);
    t1.detach();
  fprintf(stderr, "periodicDataReport thread created successfully\n");
  } if (*action_id == 3) {
    timer[0]= 5;

    // saving received buffer in global variable because then a thread will use it (safely, since it is read only)
    // if we pass it directly to the thread, since the scope is local to this function, the behaviour will be undeifined
    indication_request_buffer3 = (uint8_t *)calloc(sizeof(uint8_t), indreq_buflen);
    indication_request_length = indreq_buflen;
    memcpy(indication_request_buffer3, indreq_buff, indreq_buflen);

    fprintf(stderr, "RIC REPORT service every %i seconds \n",timer[0]);
    std::thread t2(periodicDataReport, e2sim, timer, seq_num, ric_req_id, ric_instance_id,
              ran_function_id, action_id);
    t2.detach();
  fprintf(stderr, "periodicDataReport thread created successfully\n");
  } else {
    fprintf(stderr, "RIC INSERT service \n");
    indication_request_buffer2 = (uint8_t *)calloc(sizeof(uint8_t), indreq_buflen);
    indication_request_length = indreq_buflen;
    memcpy(indication_request_buffer2, indreq_buff, indreq_buflen);
    nonPeriodicDataReport(e2sim, timer, seq_num, ric_req_id, ric_instance_id,
              ran_function_id, action_id);
  }

}

void startUnsolicitedRICIndiListener(E2Sim *e2sim, long requestorId){
    if(!report_listener_running){
      long seq_num = 1;
      std::thread t3(RICIndiListener, e2sim, seq_num, requestorId);
      t3.detach();
      report_listener_running = true;
    }
  }

// function to periodically report data
void RICIndiListener(E2Sim *e2sim, long seqNum, long requestorId)
{ 

  printf("RICIndiListener\n");

  int in_port = 6600;
  boost::asio::io_service io_service;

  // in socket (from gnb)
  boost::asio::ip::udp::socket in_socket(io_service);
  boost::asio::ip::udp::endpoint remote_endpoint_in;
  in_socket.open(boost::asio::ip::udp::v4());
  remote_endpoint_in = boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), in_port);
  in_socket.bind(remote_endpoint_in);
  //in_socket.bind(remote_endpoint_in, in_port);
  boost::system::error_code err;

  //char recvbuf[4096];
  char recvbuf[9000]; // higher size messages
  size_t recvlen;

  E2AP_PDU *e2ap_pdu = NULL;
  while(true){
    recvlen = in_socket.receive_from(boost::asio::buffer(recvbuf), remote_endpoint_in);
    fprintf(stderr, " recevied %lu bytes\n", recvlen);

    // append null terminator to payload such that the buffer can be processed by the kind-of-stupid following encoders
    recvbuf[recvlen] = '\0';

    // fprintf(stderr,"printing buf recevied from gnb:\n");
    // for(int i=0; i<recvlen; i++){
    //   fprintf(stderr, " %hhx ", recvbuf[i]);
    // }
    // fprintf(stderr, "\n");

    fprintf(stderr,"Sending report to ric\n");

    e2ap_pdu = (E2AP_PDU*)calloc(1,sizeof(E2AP_PDU));
    fprintf(stderr, "Encoding RIC Indication Report\n");
    encoding::generate_e2apv1_indication_report(e2ap_pdu, recvbuf, recvlen, requestorId, 0, 0, 0);
    fprintf(stderr, "RIC Indication Report successfully encoded\n");
    e2sim->encode_and_send_sctp_data(e2ap_pdu);

    //free(e2ap_pdu);
    seqNum++;
  }
}

// function to periodically report data
void periodicDataReport(E2Sim *e2sim, int *timer, long seqNum, long *ric_req_id, long *ric_instance_id,
                        long *ran_function_id, long *action_id)
{

  long requestorId = ric_req_id[0];
  long instanceId = ric_instance_id[0];
  long ranFunctionId = ran_function_id[0];
  long actionId = action_id[0];
  //char *payload = NULL;
  // E2AP_PDU *e2ap_pdu = (E2AP_PDU*)calloc(1,sizeof(E2AP_PDU));

  int out_port = 6655;
  int in_port = 6600;
  boost::asio::io_service io_service;

  // out socket (to gnb)
  boost::asio::ip::udp::socket out_socket(io_service);
  boost::asio::ip::udp::endpoint remote_endpoint_out;
  out_socket.open(boost::asio::ip::udp::v4());
  remote_endpoint_out = boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), out_port);

  // in socket (from gnb)
  boost::asio::ip::udp::socket in_socket(io_service);
  boost::asio::ip::udp::endpoint remote_endpoint_in;
  //in_socket.open(boost::asio::ip::udp::v4());
  remote_endpoint_in = boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), in_port);
  //in_socket.bind(remote_endpoint_in);
  //in_socket.bind(remote_endpoint_in, in_port);
  boost::system::error_code err;

  char recvbuf[4096];
  size_t recvlen;

  E2AP_PDU *e2ap_pdu = NULL;

  while (report_data_nrt_ric)
  {
    if (actionId==3) {
     timer[0]=5; 
    }

    fprintf(stderr, "periodicDataReport: timer expired for requestorId %ld, instanceId %ld, ranFunctionId %ld, actionId %ld: %d s\n",
            requestorId, instanceId, ranFunctionId, actionId, timer[0]);

    if (DEBUG)
    {
      fprintf(stderr, "Sending indication request buffer to gnb, size %d\n", indication_request_length);
      /*
      fprintf(stderr,"about to print buffer in periodic data report\n");
      for(int i=0; i<indication_request_length; i++){
          fprintf(stderr,"---%hhx\n",indication_request_buffer[i]);
      }
      fprintf(stderr,"\n");
      */
      // payload = (char*) "{\"timestamp\":1602706183796,\"slice_id\":0,\"dl_bytes\":53431,\"dl_thr_mbps\":2.39,\"ratio_granted_req_prb\":0.02,\"slice_prb\":6,\"dl_pkts\":200}";
      if (actionId == 1) {
        out_socket.send_to(boost::asio::buffer(indication_request_buffer1, indication_request_length), remote_endpoint_out, 0, err);
      } else if (actionId == 2) {
        printf("ERROR: ACTION 2 IS FALLING INTO PERIODIC FUNCT\n");
      } else if (actionId == 3) {
        out_socket.send_to(boost::asio::buffer(indication_request_buffer3, indication_request_length), remote_endpoint_out, 0, err);
      }
      
      startUnsolicitedRICIndiListener(e2sim,requestorId);
      std::chrono::seconds configured_sleep_duration(timer[0]);
      std::this_thread::sleep_for(configured_sleep_duration);
      seqNum++;
      continue; // TODO: delete code after this line

      fprintf(stderr, "Waiting for response from gnb...\n");
      int custom_sleep = 500;
      if (*action_id == 3) {
        custom_sleep =custom_sleep*5;
      }
      std::chrono::milliseconds sleep_duration(custom_sleep);
      std::this_thread::sleep_for(sleep_duration);
      recvlen = in_socket.receive_from(boost::asio::buffer(recvbuf), remote_endpoint_in);
      fprintf(stderr, " recevied %lu bytes\n", recvlen);

      // append null terminator to payload such that the buffer can be processed by the kind-of-stupid following encoders
      recvbuf[recvlen] = '\0';

      fprintf(stderr,"printing buf recevied from gnb:\n");
      for(int i=0; i<recvlen; i++){
        fprintf(stderr, " %hhx ", recvbuf[i]);
      }
      fprintf(stderr, "\n");

      fprintf(stderr,"Sending report to ric\n");
      //encode_and_send_ric_indication_report_metrics_buffer(recvbuf, seqNum, requestorId, instanceId, ranFunctionId, actionId);
      
      /*
      if(e2ap_pdu == NULL){
          e2ap_pdu = (E2AP_PDU*)calloc(1,sizeof(E2AP_PDU));
      } else {
        free(e2ap_pdu);
        e2ap_pdu = (E2AP_PDU*)calloc(1,sizeof(E2AP_PDU));
      }
      */
      e2ap_pdu = (E2AP_PDU*)calloc(1,sizeof(E2AP_PDU));
      fprintf(stderr, "Encoding RIC Indication Report\n");
      encoding::generate_e2apv1_indication_report(e2ap_pdu, recvbuf, recvlen, ric_req_id[0], 0, 0, 0);
      fprintf(stderr, "RIC Indication Report successfully encoded\n");
      e2sim->encode_and_send_sctp_data(e2ap_pdu);
      
      seqNum++;
    }
    else
    {
      //get_tx_string(&payload, LINES_TO_READ);
    }

    // send PDU
    /*
    if (payload)
    {
      fprintf(stderr, "Sending\n%s\n", payload);
      fprintf(stderr, "Encoding RIC Indication Report\n");
      // encoding::generate_e2apv1_indication_report(e2ap_pdu, payload, strlen(payload), ric_req_id[0], 0, 0, 0);
      // fprintf(stderr, "RIC Indication Report successfully encoded\n");
      // e2sim->encode_and_send_sctp_data(e2ap_pdu);

      // ASN.1 encode payload and header
      encode_and_send_ric_indication_report_metrics_buffer(payload, seqNum, requestorId, instanceId, ranFunctionId, actionId);
      seqNum++;
    }
    */

    //std::chrono::seconds sleep_duration(timer[0]);
    //std::this_thread::sleep_for(sleep_duration);
  }
  // loop thread
  // if (report_data_nrt_ric) {
  //  periodicDataReport(e2sim, timer, seqNum, ric_req_id, ric_instance_id, ran_function_id, action_id);
  //}
}

void nonPeriodicDataReport(E2Sim *e2sim, int *timer, long seqNum, long *ric_req_id, long *ric_instance_id,
                        long *ran_function_id, long *action_id)
{

  long requestorId = ric_req_id[0];
  long instanceId = ric_instance_id[0];
  long ranFunctionId = ran_function_id[0];
  long actionId = action_id[0];
  //char *payload = NULL;
  // E2AP_PDU *e2ap_pdu = (E2AP_PDU*)calloc(1,sizeof(E2AP_PDU));

  int out_port = 6655;
  int in_port = 6600;
  boost::asio::io_service io_service;

  // out socket (to gnb)
  boost::asio::ip::udp::socket out_socket(io_service);
  boost::asio::ip::udp::endpoint remote_endpoint_out;
  out_socket.open(boost::asio::ip::udp::v4());
  remote_endpoint_out = boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), out_port);

  // in socket (from gnb)
  boost::asio::ip::udp::socket in_socket(io_service);
  boost::asio::ip::udp::endpoint remote_endpoint_in;
  //in_socket.open(boost::asio::ip::udp::v4());
  remote_endpoint_in = boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), in_port);
  //in_socket.bind(remote_endpoint_in);
  //in_socket.bind(remote_endpoint_in, in_port);
  boost::system::error_code err;

  char recvbuf[4096];
  size_t recvlen;

  E2AP_PDU *e2ap_pdu = NULL;


    fprintf(stderr, "nonPeriodicDataReport: timer expired for requestorId %ld, instanceId %ld, ranFunctionId %ld, actionId %ld: %d s\n",
            requestorId, instanceId, ranFunctionId, actionId, timer[0]);

    if (DEBUG)
    {
      fprintf(stderr, "Sending indication request buffer to gnb, size %d\n", indication_request_length);
      /*
      fprintf(stderr,"about to print buffer in periodic data report\n");
      for(int i=0; i<indication_request_length; i++){
          fprintf(stderr,"---%hhx\n",indication_request_buffer[i]);
      }
      fprintf(stderr,"\n");
      */
      // payload = (char*) "{\"timestamp\":1602706183796,\"slice_id\":0,\"dl_bytes\":53431,\"dl_thr_mbps\":2.39,\"ratio_granted_req_prb\":0.02,\"slice_prb\":6,\"dl_pkts\":200}";
      out_socket.send_to(boost::asio::buffer(indication_request_buffer2, indication_request_length), remote_endpoint_out, 0, err);
      startUnsolicitedRICIndiListener(e2sim,requestorId);
      std::chrono::seconds configured_sleep_duration(timer[0]);
      std::this_thread::sleep_for(configured_sleep_duration);
    }
}

// log message on file
void log_message(char *message, char *message_type, int len)
{

  FILE *fp;
  char filename[100] = "/logs/du_l2.log";

  char buffer[26];
  int millisec;
  struct tm *tm_info;
  struct timeval tv;

  gettimeofday(&tv, NULL);

  millisec = lrint(tv.tv_usec / 1000.0); // Round to nearest millisec
  if (millisec >= 1000)
  { // Allow for rounding up to nearest second
    millisec -= 1000;
    tv.tv_sec++;
  }

  tm_info = localtime(&tv.tv_sec);

  strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);

  fp = fopen(filename, "a+");

  if (fp == NULL)
  {
    printf("ERROR: fp is NULL\n");
    return;
  }

  const int msg_len = len;
  char msg_copy[msg_len];
  strcpy(msg_copy, message);

  for (int i = 0; i < msg_len; i++)
  {
    if (message[i] == '\n')
    {
      msg_copy[i] = 'n';
    }
  }

  // print to console and log on file
  printf("%s,%03d\t%s\t%d\t%s\n", buffer, millisec, message_type, len, msg_copy);
  fprintf(fp, "%s,%03d\t%s\t%d\t%s\n", buffer, millisec, message_type, len, msg_copy);

  fclose(fp);
}

// terminate periodic thread that reports data to near real-time RIC
void stop_data_reporting_nrt_ric(void)
{
  printf("Terminating data reporting to near-real-time RIC\n");
  report_data_nrt_ric = 0;
}

// this sends udp datagrams containing buffers to gnb
void send_ricindi_to_bs(uint8_t *buffer, int buflen)
{
  fprintf(stderr, "sending udp datagram\n");
  boost::asio::io_service io_service;
  boost::asio::ip::udp::socket socket(io_service);
  boost::asio::ip::udp::endpoint remote_endpoint;
  socket.open(boost::asio::ip::udp::v4());
  int out_port = 6655;
  remote_endpoint = boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), out_port);
  boost::system::error_code err;
  socket.send_to(boost::asio::buffer(buffer, buflen), remote_endpoint, 0, err);
}
