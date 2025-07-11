#include "ymodem.h"


static uint16_t UpdateCRC16(uint16_t crcIn, uint8_t byte)
{
    uint32_t crc = crcIn;
    uint32_t in = byte | 0x100;

    do
    {
        crc <<= 1;
        in <<= 1;

        if(in  & 0x100)   ++crc;
        if(crc & 0x10000) crc ^= 0x1021;
    } while (!(in & 0x10000));

    return (crc & 0xffffu);
}

static uint16_t Cal_CRC16(const uint8_t* data, uint32_t size)
{
    uint32_t crc = 0;
    const uint8_t* dataEnd = data + size;
    while(data < dataEnd) crc = UpdateCRC16(crc, *data++);
    crc = UpdateCRC16(crc, 0);
    crc = UpdateCRC16(crc, 0);
    return (crc & 0xffffu);
}

int Bootloader::Ymodem_Receive_Packet(uint8_t* pdata, int* length, uint32_t timeout)
{
    *length = 0;

    /* 帧头(SOH /STX) 1B | 包号(PN) 1B | 包号反码(XPN) 1B | 信息块(DATA) 1024B | 校验(CRC) 2B*/
    uint8_t c; if(Ymodem_RecvByte(&c, timeout) != true) return -1;

    /* 判断帧头 */
    uint32_t packet_size;
    switch (c)
    {
    case SOH: packet_size = PACKET_SIZE_128;  break;
    case STX: packet_size = PACKET_SIZE_1024; break;
    case EOT: return 0;
    case CA: if((Ymodem_RecvByte(&c, timeout) == true) && (c == CA)) { *length = -1; return 0; } else return -1;
    case ABORT1: return 1;
    case ABORT2: return 1;
    default: return -1;
    }

    /* 接收数据 */
    *pdata = c;
    for (uint16_t i=1; i<(packet_size + PACKET_OVERHEAD); i++) if(Ymodem_RecvByte(pdata + i, timeout) != true) return -1;

    /* 检验序号补码是否正确 */
    if (pdata[PACKET_SEQNO_INDEX] != ((pdata[PACKET_SEQNO_COMP_INDEX] ^ 0xff) & 0xff)) return -1;
    
    /* CRC */
    uint16_t computedcrc = Cal_CRC16(&pdata[PACKET_HEADER], packet_size);
    if (computedcrc != (uint16_t)((pdata[packet_size + 3] << 8) | pdata[packet_size + 4])) return -1;
    
    *length = packet_size;
    return 0;
}

int Bootloader::Ymodem_Receive()
{
    uint32_t CurAddr = APP_startAddr;
    uint8_t packet_data[PACKET_SIZE_1029] = {0};
    int packets_received = 0;
    int session_begin=0; // 文件接收会话开始
    int session_done=0;  // 文件接收会话完成
    int file_done=0;     // 文件接收完毕
    int errors=0;        // 错误次数

    /* 临时变量 */
    int packet_length = 0;
    while (true)
    {
        packets_received = 0;
        file_done = 0;
        
        while (true)
        {
            switch (Ymodem_Receive_Packet(packet_data, &packet_length, NAK_TIMEOUT))
            {
            case 0:
                errors = 0;
                switch (packet_length)
                {
                case -1: Ymodem_SendByte(ACK); return 0; // Abort by sender
                case  0: Ymodem_SendByte(ACK); file_done = 1; break; // End of transmission
                default:
                    if((packet_data[PACKET_SEQNO_INDEX] & 0xFF) != (packets_received & 0xFF)) Ymodem_SendByte(NAK); // 检查包序号
                    else
                    {
                        if (packets_received == 0)
                        {
                            /* 文件名 */
                            if (packet_data[PACKET_HEADER] != 0x00)
                            {
                                /* 文件名 */
                                char* pName = (char*)(packet_data+PACKET_HEADER);
                                for (int i=0; (*pName!=0x00) && (i<FILE_NAME_LENGTH); pName++ ) {
                                    name[i++] = *pName;
                                }

                                /* 文件大小 */
                                char* pSize = pName+1;
                                for (int i=0; (*pSize!=0x00) && (i<FILE_SIZE_LENGTH); pSize++ ) {
                                    size[i++] = *pSize;
                                }

                                /* 文件大小检查 */
                                if (APPSizeCheck(atoi((char*)size)) == false) {
                                    return Ymodem_CancelTransfer(-1);
                                }

                                Ymodem_SendByte(ACK);
                                Ymodem_SendByte(CharC);
                            }
                            else {
                                /* 空文件名 */
                                Ymodem_SendByte(ACK);
                                file_done = 1;
                                session_done = 1;
                                break;
                            }
                        }
                        else {
                            if(FlashProgram(CurAddr, packet_data+PACKET_HEADER, packet_length) == true) {
                                CurAddr += packet_length;
                                Ymodem_SendByte(ACK);
                            }
                            else return Ymodem_CancelTransfer(-2);
                        }
                        packets_received++;
                        session_begin = 1;
                    }
                    break;
                }
                break;

            case 1:
                return Ymodem_CancelTransfer(-3);

            default: // 校验错误
                if (session_begin > 0)
                {
                    errors++;
                }

                if (errors > MAX_ERRORS) {
                    return Ymodem_CancelTransfer(0);
                }
                Ymodem_SendByte(CharC); // 发送校验值
                break;
            }

            /* 文件接收完成，内层循环才会终止接收数据包 */
            if(file_done != 0) break;
        }

        /* 外层循环退出条件，确保文件接收的完整过程，包括初始化、数据接收以及最终的会话结束。 */
        if(session_done != 0) break; // 文件发送完成
    }
    return 0;
}