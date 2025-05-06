#include "arch/allwinner/v3s_platform.h"
#include "arch/allwinner/v3s_ccmu.h"


/*
 * \brief SD Global Control Register
 */
#define SDMMC_SCCR_CLK_ENB         (0x01 << 16)     // 0: Disable clock 1: Enable clock
#define SDMMC_SCCR_CCLK_CTRL       (0x01 << 17)     // Card Clock Output Control 0: Card clock always on  1: Turn off card clock when FSM in IDLE state
#define SDMMC_SCCR_MASK_DATA0      (0x01 << 31)     // 0: Do not mask data0 when updata clock   1: Mask data0 when updata clock
#define __CCLK_DIV_SHIFT__         (0)              // Card clock divider shift
#define SSDMMC_SET_CCLK_DIV(x)     ( ((x) << __CCLK_DIV_SHIFT__) & 0xFF)    // Card clock divider: n – Source clock is divided by 2*n.(n=0~255)

/*
 * \brief SD Global Control Register
 */
#define SDMMC_SGCR_SOFT_RST        (0x01 << 0)     // 0: No reset 1: Reset SD/MMC controller
#define SDMMC_SGCR_FIFO_RST        (0x01 << 1)     // 0: No reset 1: Reset FIFO
#define SDMMC_SGCR_DMA_RST         (0x01 << 2)     // 0: No reset 1: Reset DMA 
#define SDMMC_SGCR_INT_ENB         (0x01 << 4)     // 0: disable interrupts 1: Enable interrupts
#define SDMMC_SGCR_DMA_ENB         (0x01 << 5)     // 0: Disable DMA to transfer data, using AHB bus 1: Enable DMA to transfer data
#define SDMMC_SGCR_DBC_ENB         (0x01 << 8)     // 0: disable de-bounce 1: enable de-bounce
#define SDMMC_SGCR_DDR_SEL         (0x01 << 10)    // 0: SDR mode 1: DDR mode
#define SDMMC_SGCR_AC_MODE         (0x01 << 31)    // 0: DMA bus 1: AHB bus


/*
 * \brief SD Command Register
 */
#define SDMMC_CMD_LOAD             (0x01 << 31)    // 0: No load command 1: Load command
#define SDMMC_USE_HOLD_REG         (0x01 << 29)    // 0: No hold register 1: Hold register
#define SDMMC_VOL_SW               (0x01 << 28)    // 0: No voltage switch 1: Voltage switch
#define SDMMC_PRG_CLK              (0x01 << 21)    // 0: Normal command 1: Change Card Clock; when this bit is set, controller will change clock 
                                               // domain and clock output. No command will be sent.
#define SDMMC_SEND_INIT_SEQ        (0x01 << 15)    // 0: No init sequence 1: Init sequence
#define SDMMC_STOP_ABT_CMD         (0x01 << 14)    // 0: Normal command 1: send Stop or abort command to stop current data transfer in progress
#define SDMMC_WAIT_PRE_OVER        (0x01 << 13)    // 0: No wait for previous command to finish 1: Wait for previous command to finish
#define SDMMC_STOP_CMD_FLAG        (0x01 << 12)    // 0: No auto send stop command 1: Auto send stop command
#define SDMMC_TRANS_MODE           (0x01 << 11)    // 0: Block mode 1: Stream mode
#define SDMMC_TRANS_DIR            (0x01 << 10)    // 0: Read Operation 1: Write Operation
#define SDMMC_DATA_TRANS           (0x01 << 9)     // 0: No data transfer 1: Data transfer
#define SDMMC_CHK_RESP_CRC         (0x01 << 8)     // 0: No CRC check 1: CRC check
#define SDMMC_LONG_RESP            (0x01 << 7)     // 0: Short response 1: Long response
#define SDMMC_RESP_RCV             (0x01 << 6)     // 0: No response 1: Response


/*
 * \brief SD Raw Interrupt Status Register
 */
#define SDMMC_RISR_RESP_ERR        (0x01 << 1)     // Response Error (no response or response CRC error)
#define SDMMC_RISR_COMMAND_OK      (0x01 << 2)     // Command Complete
#define SDMMC_RISR_TRANS_OK        (0x01 << 3)     // Data Transfer Complete
#define SDMMC_RISR_TRANS_REQ       (0x01 << 4)     // Data Transmit Request
#define SDMMC_RISR_RECV_REQ        (0x01 << 5)     // Data Receive Request
#define SDMMC_RISR_RESP_CRC_ERR    (0x01 << 6)     // Response CRC error
#define SDMMC_RISR_DATA_CRC_ERR    (0x01 << 7)     // Data CRC error
#define SDMMC_RISR_RESP_TIMEOUT    (0x01 << 8)     // Response timeout/Boot ACK received
#define SDMMC_RISR_DATA_TIMEOUT    (0x01 << 9)     // Data timeout/Boot data start
#define SDMMC_RISR_SWITCH_OK       (0x01 << 10)    // Data starvation timeout (HTO)/V1.8 Switch Done
#define SDMMC_RISR_FIFO_OVERFLOW   (0x01 << 11)    // FIFO under run/overflow
#define SDMMC_RISR_CMD_BUSY        (0x01 << 12)    // Command Busy and illegal write
#define SDMMC_RISR_START_ERR       (0x01 << 13)    // Data Start Error
#define SDMMC_RISR_AUTO_CMD_OK     (0x01 << 14)    // Auto command done
#define SDMMC_RISR_END_ERR         (0x01 << 15)    // Data End-bit error
#define SDMMC_RISR_SDIO_INT        (0x01 << 16)    // SDIO interrupt
#define SDMMC_RISR_CARD_INSERTED   (0x01 << 30)    // card inserted
#define SDMMC_RISR_CARD_REMOVED    (0x01 << 31)    // card removed


/*
 * \brief SD Interrupt Mask Register
 */
#define SDMMC_SIMR_RESP_ERR        (0x01 << 1)     // Response Error (no response or response CRC error)
#define SDMMC_SIMR_COMMAND_OK      (0x01 << 2)     // Command Complete
#define SDMMC_SIMR_TRANS_OK        (0x01 << 3)     // Data Transfer Complete
#define SDMMC_SIMR_TRANS_REQ       (0x01 << 4)     // Data Transmit Request
#define SDMMC_SIMR_RECV_REQ        (0x01 << 5)     // Data Receive Request
#define SDMMC_SIMR_RESP_CRC_ERR    (0x01 << 6)     // Response CRC error
#define SDMMC_SIMR_DATA_CRC_ERR    (0x01 << 7)     // Data CRC error
#define SDMMC_SIMR_RESP_TIMEOUT    (0x01 << 8)     // Response timeout/Boot ACK received
#define SDMMC_SIMR_DATA_TIMEOUT    (0x01 << 9)     // Data timeout/Boot data start
#define SDMMC_SIMR_SWITCH_OK       (0x01 << 10)    // Data starvation timeout (HTO)/V1.8 Switch Done
#define SDMMC_SIMR_FIFO_OVERFLOW   (0x01 << 11)    // FIFO under run/overflow
#define SDMMC_SIMR_CMD_BUSY        (0x01 << 12)    // Command Busy and illegal write
#define SDMMC_SIMR_START_ERR       (0x01 << 13)    // Data Start Error
#define SDMMC_SIMR_AUTO_CMD_OK     (0x01 << 14)    // Auto command done
#define SDMMC_SIMR_END_ERR         (0x01 << 15)    // Data End-bit error
#define SDMMC_SIMR_SDIO_INT        (0x01 << 16)    // SDIO interrupt
#define SDMMC_SIMR_CARD_INSERTED   (0x01 << 30)    // card inserted
#define SDMMC_SIMR_CARD_REMOVED    (0x01 << 31)    // card removed



/*
 * \brief SD Status Register
 */
#define SDMMC_SSR_FIFO_RX_LEVEL    (0x01 << 0)      // FIFO reached receive trigger leve
#define SDMMC_SSR_FIFO_TX_LEVEL    (0x01 << 1)      // FIFO reached transmit trigger level
#define SDMMC_SSR_FIFO_RX_EMPTY    (0x01 << 2)      // FIFO empty
#define SDMMC_SSR_FIFO_TX_EMPTY    (0x01 << 3)      // FIFO full
#define SDMMC_SSR_CARD_PRESENT     (0x01 << 8)      // Card present
#define SDMMC_SSR_CARD_BUSY        (0x01 << 9)      // Card busy
#define SDMMC_SSR_FSM_BUSY         (0x01 << 10)     // Data transmit or receive state-machine is busy
#define SDMMC_SSR_DMA_REQ          (0x01 << 31)     // DMA request signal state

#define _SDMMC_RESP_IDX_SHIFT_     (11)
#define _SDMMC_RESP_IDX_MASK_      (0x3f)
/* \brief Index of previous response, including any auto-stop sent by controller */
#define SDMMC_GET_PREV_IDX(ssr)    (((ssr) >> _SDMMC_RESP_IDX_SHIFT_) & _SDMMC_RESP_IDX_MASK_)

#define _SDMMC_FIFO_LEVEL_SHIFT_   (17)
#define _SDMMC_FIFO_LEVEL_MASK_    (0x1f)
/* \brief Number of filled locations in FIFO */
#define SDMMC_GET_FIFO_LEVEL(ssr)  (((ssr) >> _SDMMC_FIFO_LEVEL_SHIFT_) & _SDMMC_FIFO_LEVEL_MASK_)

