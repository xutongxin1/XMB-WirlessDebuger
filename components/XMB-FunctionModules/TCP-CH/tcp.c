
#include "sdkconfig.h"

#include <string.h>
#include <stdint.h>
#include <sys/param.h>
#include <stdatomic.h>

#include "InstructionServer/wifi_configuration.h"
#include "UART/uart_val.h"
#include "TCP-CH/tcp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/uart.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netbuf.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include "lwip/priv/api_msg.h"
#include <lwip/netdb.h>

#define EVENTS_QUEUE_SIZE 10

#ifdef CALLBACK_DEBUG
#define debug(s, ...) printf("%s: " s "\n", "Cb:", ##__VA_ARGS__)
#else
#define debug(s, ...)
#endif
#define MY_IP_ADDR(...) IP4_ADDR(__VA_ARGS__)
static const char *TCP_TAG = "TCP";
static err_t get_connect_state(struct netconn *conn);
//static TaskHandle_t* SubTask_Handle;
//static TaskHandle_t SubTask_Handle_1;
struct netconn *conn_All = NULL;
struct netconn *newconn_All = NULL;
//static uint8_t subtask_one_flag = 0;
//static TaskHandle_t** TcpTaskHandle;
unsigned int TcpHandle_FatherTask_current = 0;
TcpTaskHandle_t TCP_TASK_HANDLE[12];
SubTcpParam SubParam = {
    .TcpParam = NULL,
    .conn = NULL,
    .newconn = NULL,
    .son_task_current = 0,
};
// static uint16_t choose_port(uint8_t pin)
// {
//     switch (pin)
//     {
//     case CH2:
//         return 1922;
//     case CH3:
//         return 1923;
//     case CH1:
//         return 1921;
//     default:
//         return -1;
//     }
// }
/**
 * @ingroup xmb tcp server
 * Create a tcp server according to the configuration parameters
 * It is an internal function
 * @param Parameter the configuration parameters
 * @param conn A netconn descriptor
 * @return ERR_OK if bound, any other err_t on failure
 */
uint8_t create_tcp_server(uint16_t port, struct netconn **conn) {
    while (*conn == NULL) {
        *conn = netconn_new(NETCONN_TCP);
    }
    printf("creat : %p\r\n", *conn);
    // netconn_set_nonblocking(conn, NETCONN_FLAG_NON_BLOCKING);
    //netconn_bind(*conn, &ip_info.ip, Param->port);
    /* Bind connection to well known port number 7. */
    netconn_bind(*conn, IP_ADDR_ANY, port);
    netconn_listen(*conn); /* Grab new connection. */
    printf("PORT: %d\nLISTENING.....\n", port);
    return ESP_OK;
}

void tcp_send_server(TcpParam *Parameter) {
    // printf("Parameter = %p  \n", Parameter);
    TcpParam *Param = Parameter;
    // printf("Param = %p  \n", Param);
    struct netconn *conn = NULL;
    // printf("conn\n");
    struct netconn *newconn = NULL;

    err_t err = 1;
    // uint16_t len = 0;
    // void *recv_data;
    // recv_data = (void *)pvPortMalloc(TCP_BUF_SIZE);
    // printf("Param->uart_queue = %p  \n*Param->uart_queue = %p\n", Param->uart_queue, *Param->uart_queue);
    QueueHandle_t buff_queue = *Param->tx_buff_queue;
    //tcpip_adapter_ip_info_t ip_info;
    /* Create a new connection identifier. */
    /* Bind connection to well known port number 7. */
    create_tcp_server(Param->port, &conn);
    // while (conn == NULL )
    // {
    //     conn = netconn_new(NETCONN_TCP);
    // }
    // // netconn_set_nonblocking(conn, NETCONN_FLAG_NON_BLOCKING);
    // //netconn_bind(*conn, &ip_info.ip, Param->port);
    //  /* Bind connection to well known port number 7. */
    // netconn_bind(conn, IP_ADDR_ANY, Param->port);
    // netconn_listen(conn); /* Grab new connection. */
    // netconn_set_nonblocking(conn, NETCONN_FLAG_NON_BLOCKING);
    //MY_IP_ADDR(&ip_info.ip, TCP_IP_ADDRESS);
    // netconn_bind(conn, &ip_info.ip, Param->port);

    /* Tell connection to go into listening mode. */
    //netconn_listen(conn);
    //printf("listening : %p\r\n",conn);

    /* Grab new connection. */
    while (1) {
        int re_err;
        err = netconn_accept(conn, &newconn);

        /* Process the new connection. */

        if (err == ERR_OK) {

            struct netbuf *buf;
            struct netbuf *buf2;
            void *data;
            while (1) {
                re_err = (netconn_recv(newconn, &buf));
                if (re_err == ERR_OK) {
                    do {
                        events event;
                        netbuf_data(buf, &data, &event.buff_len);
                        event.buff = data;
                        if (xQueueSend(buff_queue, &event, pdMS_TO_TICKS(10)) == pdPASS)
                            ESP_LOGE(TCP_TAG, "SEND TO QUEUE FAILD\n");
                    } while ((netbuf_next(buf) >= 0));
                    netbuf_delete(buf2);
                    re_err = (netconn_recv(newconn, &buf2));
                    if (re_err == ERR_OK) {
                        do {
                            events event;
                            netbuf_data(buf2, &data, &event.buff_len);
                            event.buff = data;
                            if (xQueueSend(buff_queue, &event, pdMS_TO_TICKS(10)) == pdPASS)
                                ESP_LOGE(TCP_TAG, "SEND TO QUEUE FAILD\n");
                        } while ((netbuf_next(buf2) >= 0));
                        netbuf_delete(buf);
                    }
                } else if (re_err == ERR_CLSD) {
                    ESP_LOGE(TCP_TAG, "DISCONNECT PORT:%d\n", Param->port);
                    //netbuf_delete(buf);
                    //netbuf_delete(buf2);
                    break;
                }
            }
            netconn_close(newconn);
            netconn_delete(newconn);
            netconn_listen(conn);
        }
    }
    vTaskDelete(NULL);
}
void tcp_rev_server(TcpParam *Parameter) {
    TcpParam *Param = Parameter;
    struct netconn *conn = NULL;
    struct netconn *newconn = NULL;
    // err_t re_err = 0;
    QueueHandle_t buff_queue = *Param->rx_buff_queue;

    /* Create a new connection identifier. */
    create_tcp_server(Param->port, &conn);
    // while (conn == NULL)
    // {
    //     conn = netconn_new(NETCONN_TCP);
    //     printf("CONN: %p\n", conn);
    //     conn->send_timeout = 0;
    // }
    // tcpip_adapter_ip_info_t ip_info;
    // MY_IP_ADDR(&ip_info.ip, TCP_IP_ADDRESS);
    // // netconn_set_nonblocking(conn, NETCONN_FLAG_NON_BLOCKING);
    // netconn_bind(conn, &ip_info.ip, Param->port);
    // netconn_bind(conn, IP4_ADDR_ANY, Param->port);
    /* Tell connection to go into listening mode. */
    //netconn_listen(conn);
    //printf("PORT: %d\nLISTENING.....\n", Param->port);
    /* Grab new connection. */
    while (1) {
        /* Process the new connection. */
        if (netconn_accept(conn, &newconn) == ERR_OK) {
            while (1) {
                events event;
                int ret = xQueueReceive(buff_queue, &event, portMAX_DELAY);
                if (ret == pdTRUE) {
                    netconn_write(newconn, event.buff, event.buff_len, NETCONN_NOCOPY);
                }
                if (get_connect_state(newconn) == ERR_CLSD) {
                    netconn_close(newconn);
                    netconn_delete(newconn);
                    netconn_listen(conn);
                    ESP_LOGE(TCP_TAG, "DISCONNECT PORT:%d\n", Param->port);
                    break;
                }
            }
        }
    }
    vTaskDelete(NULL);
}

void tcp_server_subtask(SubTcpParam *Parameter) {

    TcpParam *tcp_param = Parameter->TcpParam;
    QueueHandle_t rx_buff_queue = *tcp_param->rx_buff_queue;
    printf("tcp_rx_queue rx: %p\n", tcp_param->rx_buff_queue);
    //struct netconn *conn = Param->conn;
    struct netconn *newconn = *(Parameter->newconn);
    while (TCP_TASK_HANDLE[Parameter->son_task_current].SonTask_exists) {

        events event;
        int ret = xQueueReceive(rx_buff_queue, &event, portMAX_DELAY);
        if (ret == pdTRUE) {
            netconn_write(newconn, event.buff, event.buff_len, NETCONN_NOCOPY);
        }
    }
    vTaskDelete(NULL);
}

TcpTaskHandle_t *TcpTaskCreate(TcpParam *Parameter) {
    printf("Parameter rx_buff_queue:%p\n",Parameter->rx_buff_queue);
    printf("Parameter tx_buff_queue:%p\n",Parameter->tx_buff_queue);
    printf("Param:%p\n",Parameter);
    const char allname[] = "ALL";
    const char rxname[] = "Rec";
    const char txname[] = "Tran";
    char pcName[18];
    printf("\nParam->mode:%d\n", Parameter->mode);
    switch (Parameter->mode) {
        case SEND:sprintf(pcName, "Tcp%s%d", txname, TcpHandle_FatherTask_current);
            xTaskCreatePinnedToCore(tcp_send_server,
                                    (const char *const) pcName,
                                    5120,
                                    Parameter,
                                    14,
                                    TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTaskHandle,
                                    0);
            TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTask_exists = true;
            TCP_TASK_HANDLE[TcpHandle_FatherTask_current].mode = SEND;
            TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTaskPort = Parameter->port;
            TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTaskcount = TcpHandle_FatherTask_current;
            TcpHandle_FatherTask_current++;
            break;
        case RECEIVE:sprintf(pcName, "Tcp%s%d", rxname, TcpHandle_FatherTask_current);
            xTaskCreatePinnedToCore(tcp_rev_server,
                                    (const char *const) pcName,
                                    5120,
                                    Parameter,
                                    14,
                                    TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTaskHandle,
                                    0);
            TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTask_exists = true;
            TCP_TASK_HANDLE[TcpHandle_FatherTask_current].mode = RECEIVE;
            TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTaskPort = Parameter->port;
            TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTaskcount = TcpHandle_FatherTask_current;
            TcpHandle_FatherTask_current++;
            break;
        case ALL:sprintf(pcName, "Tcp%s%d", allname, TcpHandle_FatherTask_current);
            printf("%s", pcName);
            xTaskCreatePinnedToCore(tcp_server_rev_and_send,
                                    (const char *const) pcName,
                                    5120,
                                    Parameter,
                                    14,
                                    TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTaskHandle,
                                    0);
            if (TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTaskHandle != NULL) {
                TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTask_exists = true;
                TCP_TASK_HANDLE[TcpHandle_FatherTask_current].mode = ALL;
                TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTaskPort = Parameter->port;
            }
            TCP_TASK_HANDLE[TcpHandle_FatherTask_current].FatherTaskcount = TcpHandle_FatherTask_current;
            TcpHandle_FatherTask_current++;
            break;
        default:break;
    }
    return TCP_TASK_HANDLE;
}

uint8_t TcpTaskAllDelete(TcpTaskHandle_t *TCP_TASK_HANDLE_delete) {
    unsigned int need_to_delete_taskcount = 0;
    need_to_delete_taskcount = TcpHandle_FatherTask_current;
    for (int i = 0; i <= need_to_delete_taskcount; i++) {
        if ((TCP_TASK_HANDLE_delete[i].FatherTask_exists) == true) {
            if (TCP_TASK_HANDLE_delete[i].SonTaskHandle != NULL) {
                TCP_TASK_HANDLE_delete[i].SonTask_exists = false;
                vTaskDelete(*(TCP_TASK_HANDLE_delete[i].SonTaskHandle));

            }
            netconn_delete(newconn_All);
            netconn_delete(conn_All);
            vTaskDelete(*(TCP_TASK_HANDLE_delete[i].FatherTaskHandle));
            printf("\nDelete%d\n",i);
            TCP_TASK_HANDLE_delete[i].FatherTaskcount = 0;
            TCP_TASK_HANDLE_delete[i].mode = 0;
            TCP_TASK_HANDLE_delete[i].FatherTaskPort = 0;
            TCP_TASK_HANDLE_delete[i].FatherTask_exists = false;
            TcpHandle_FatherTask_current = 0;
        }
    }

    return ESP_OK;
}

///
/// \param Parameter
void tcp_server_rev_and_send(TcpParam *Parameter) {

    err_t err = 1;
    char tmp[16];
    printf("tcp_tx_queue tx: %p\n", Parameter->tx_buff_queue);
    /* Create a new connection identifier. */
    create_tcp_server(Parameter->port, &conn_All);

    SubParam.TcpParam = Parameter;
    SubParam.conn = &conn_All;
    SubParam.newconn = &newconn_All;
    SubParam.son_task_current = TcpHandle_FatherTask_current;

    /* Tell connection to go into listening mode. */
    //netconn_listen(conn);
    // printf("PORT: %d\nLISTENING.....\n",  Param->port);
    while (1) {
        int re_err;
        err = netconn_accept(conn_All, &newconn_All);
        /* Process the new connection. */

        if (err == ERR_OK) {

            if (!TCP_TASK_HANDLE[TcpHandle_FatherTask_current].SonTask_exists) {
                /*Create Receive Subtask*/
                SubParam.newconn = &newconn_All;
                sprintf(tmp, "tcp_subtask_%d", Parameter->port);
                printf("\n%s\n", tmp);

                if (xTaskCreatePinnedToCore(tcp_server_subtask,
                                            (const char *const) tmp,
                                            4096,
                                             (&SubParam),
                                            14,
                                            TCP_TASK_HANDLE[TcpHandle_FatherTask_current].SonTaskHandle, 0) == pdPASS) {
                    TCP_TASK_HANDLE[TcpHandle_FatherTask_current].SonTask_exists = true;
                    TCP_TASK_HANDLE[TcpHandle_FatherTask_current].SonTaskcount++;
                    //strcpy(TCP_TASK_HANDLE[TcpHandle_FatherTask_current].SonTaskname, tmp);
                }
            }
            /*Create send buffer*/

            void *data;
            while (conn_All->state != NETCONN_CLOSE) {
                struct netbuf *buf;
                re_err = (netconn_recv(newconn_All, &buf));
                if (re_err == ERR_OK) {
                    do{
                        events tx_event_1;
                        netbuf_data(buf, &data, &tx_event_1.buff_len);
                        tx_event_1.buff = data;
//                        if (newconn_All->state != NETCONN_CLOSE) {
//                            netbuf_delete(buf);
//                            break;
//                        }
                        if (xQueueSend(*(Parameter->tx_buff_queue), &tx_event_1, pdMS_TO_TICKS(10)) == pdPASS) {

                        }
                        else
                            ESP_LOGE(TCP_TAG, "SEND TO QUEUE FAILD\n");

                    }while ((netbuf_next(buf) >= 0));
                        netbuf_delete(buf);

                } else if (re_err == ERR_CLSD) {
                    ESP_LOGE(TCP_TAG, "DISCONNECT PORT:%d\n", Parameter->port);

                }

            }

//            vTaskDelete(*TCP_TASK_HANDLE[TcpHandle.FatherTaskcount].SonTaskHandle);
//            is_created_tasks = 0;
//            netconn_close(newconn);
//            netconn_delete(newconn);
//            netconn_listen(conn);
        }
    }
}

static err_t get_connect_state(struct netconn *conn) {
    void *msg;
    err_t err;
    if (sys_arch_mbox_tryfetch(&conn->recvmbox, &msg) != SYS_MBOX_EMPTY) {
        lwip_netconn_is_err_msg(msg, &err);
        if (err == ERR_CLSD) {
            return ERR_CLSD;
        }
    }
    return ERR_OK;
}