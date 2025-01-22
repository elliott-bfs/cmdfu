#include "mdfu/checksum.h"

/**
 * @brief Calculate inverted 16-bit two's complement frame checksum.
 * 
 * Pads data implicitly with zero byte to calulate checksum.
 * This function will not work on big-endian machines.
 * 
 * @param [in] data - Pointer to data for checksum calculation.
 * @param [in] size - Number of bytes in data.
 * @return uint16_t - Frame check sequence.
 */
uint16_t calculate_crc16(int size, uint8_t *data)
{
    uint16_t checksum = 0U;

    for (uint16_t index = 0; index < size; index++)
    {
        uint8_t nextByte = data[index];

        if ((index % 2) == 0)
        {
            checksum += (uint16_t) nextByte;
        }
        else
        {
            checksum += ((uint16_t) nextByte) << 8;
        }
    }
    return ~checksum;
}