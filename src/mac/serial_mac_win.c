#include <Windows.h>
#include <errhandlingapi.h>
#include <winbase.h>
#undef ERROR
#include "mdfu/logging.h"
#include "mdfu/mac/serial_mac.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define PORT_NAME_MAX_SIZE 256

static bool opened = false;
static char port[PORT_NAME_MAX_SIZE + 1];
static int baudrate = 0;
static HANDLE hSerial = NULL;
static COMMTIMEOUTS timeouts = {0};
static DCB params = {0};

static int mac_init(void *conf) {
  struct serial_config *config = (struct serial_config *)conf;
  if (opened) {
    ERROR("Cannot initialize while MAC is opened.");
    errno = EBUSY;
    return -1;
  }
  DEBUG("Initializing serial MAC");
  int port_name_size = strlen(config->port);
  if (PORT_NAME_MAX_SIZE < port_name_size) {
    ERROR("This driver only supports serial port name lenght of max 256 "
          "characters");
    errno = EINVAL;
    return -1;
  }
  sprintf(port, "\\\\.\\%s", config->port);
  baudrate = config->baudrate;
  return 0;
}

static int mac_open(void) {
  DEBUG("Opening serial MAC");
  if (opened) {
    errno = EBUSY;
    return -1;
  }

  hSerial = CreateFile(port,                         // port name
                       GENERIC_READ | GENERIC_WRITE, // Read/Write
                       0,                            // No Sharing
                       NULL,                         // No Security
                       OPEN_EXISTING,                // Open existing port only
                       0,                            // Non Overlapped I/O
                       NULL);                        // Null for Comm Devices

  if (hSerial == INVALID_HANDLE_VALUE) {
    LPSTR messageBuffer = NULL;
    DWORD errorId = GetLastError();
    // Ask Win32 to give us the string version of the error code.
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, errorId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPSTR)&messageBuffer, 0, NULL);
    ERROR("Serial MAC CreateFile: 0x%08X %s %s", (uint32_t)errorId,
          messageBuffer, port);
    return -1;
  }

  // Configure read and write operations to time out after 100 ms.
  timeouts.ReadIntervalTimeout = 0;
  timeouts.ReadTotalTimeoutConstant = 100;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 100;
  timeouts.WriteTotalTimeoutMultiplier = 0;

  if (!SetCommTimeouts(hSerial, &timeouts)) {
    LPSTR messageBuffer = NULL;
    DWORD errorId = GetLastError();
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, errorId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPSTR)&messageBuffer, 0, NULL);
    ERROR("Serial MAC SetCommTimeouts: 0x%08X %s", (uint32_t)errorId,
          messageBuffer);
    CloseHandle(hSerial);
    return -1;
  }

  // Set the baud rate and other options.
  params.DCBlength = sizeof(params);
  if (!GetCommState(hSerial, &params)) {
    LPSTR messageBuffer = NULL;
    DWORD errorId = GetLastError();
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, errorId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPSTR)&messageBuffer, 0, NULL);
    ERROR("Serial MAC GetCommState: 0x%08X %s", (uint32_t)errorId,
          messageBuffer);
    CloseHandle(hSerial);
    return -1;
  }
  params.BaudRate = baudrate;               // Set Baud Rate
  params.ByteSize = 8;                      // Data Size = 8 bits
  params.StopBits = ONESTOPBIT;             // One Stop Bit
  params.Parity = NOPARITY;                 // No Parity
  params.fDtrControl = DTR_CONTROL_DISABLE; // Disable DTR
  params.fRtsControl = RTS_CONTROL_DISABLE; // Disable RTS
  params.fOutxCtsFlow = FALSE;              // Disable CTS output flow control
  params.fOutxDsrFlow = FALSE;              // Disable DSR output flow control
  params.fDsrSensitivity = FALSE;           // Disable DSR sensitivity
  params.fOutX = FALSE;         // Disable XON/XOFF output flow control
  params.fInX = FALSE;          // Disable XON/XOFF input flow control
  params.fErrorChar = FALSE;    // Disable error replacement
  params.fNull = FALSE;         // Disable null stripping
  params.fBinary = TRUE;        // Enable binary mode
  params.fAbortOnError = FALSE; // Do not abort on error
  params.fTXContinueOnXoff =
      TRUE; // Continue transmitting when XOFF is received

  if (!SetCommState(hSerial, &params)) {
    LPSTR messageBuffer = NULL;
    DWORD errorId = GetLastError();
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, errorId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPSTR)&messageBuffer, 0, NULL);
    ERROR("Serial MAC SetCommState: 0x%08X %s", (uint32_t)errorId,
          messageBuffer);
    CloseHandle(hSerial);
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, GetLastError(), 0, port, PORT_NAME_MAX_SIZE, NULL);
    return -1;
  }

  opened = true;
  return 0;
}

static int mac_close(void) {
  DEBUG("Closing serial MAC");
  if (opened) {
    CloseHandle(hSerial);
    opened = false;
    return 0;
  } else {
    errno = EBADF;
    return -1;
  }
}

static int mac_read(int size, uint8_t *data) {
  DWORD received;
  if (!ReadFile(hSerial, data, size, &received, NULL)) {
    LPSTR messageBuffer = NULL;
    DWORD errorId = GetLastError();
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, errorId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPSTR)&messageBuffer, 0, NULL);
    ERROR("Serial MAC read: 0x%08X %s", (uint32_t)errorId, messageBuffer);
    return -1;
  }
  return (int)received;
}

static int mac_write(int size, uint8_t *data) {
  DWORD written;
  if (!WriteFile(hSerial, data, size, &written, NULL)) {
    LPSTR messageBuffer = NULL;
    DWORD errorId = GetLastError();
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, errorId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPSTR)&messageBuffer, 0, NULL);
    ERROR("Serial MAC write: 0x%08X %s", (uint32_t)errorId, messageBuffer);
  }
  return (int)written;
}

mac_t serial_mac = {.open = mac_open,
                    .close = mac_close,
                    .init = mac_init,
                    .write = mac_write,
                    .read = mac_read};

void get_serial_mac(mac_t **mac) { *mac = &serial_mac; }
