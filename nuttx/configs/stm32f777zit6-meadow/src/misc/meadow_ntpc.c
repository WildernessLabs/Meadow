#include <nuttx/config.h>
#include <meadow/meadow_ntpc.h>

#include "../ntpclient/ntpclient.h"

/****************************************************************************
 * Name: meadow_ntpc_start
 *
 * Description:
 *   This function is a wrapper for the Meadow ntpc_start custom 
 *   implementation, which is responsible for starting the NTP client 
 *   process, initializing the NTP client, and beginning the time 
 *   synchronization process with the NTP server, taking into consideration
 *   the meadow.config.yaml file.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   Zero if the function succeeds, otherwise returns a specific NuttX
 *   error code corresponding to the encountered issue.
 *
 * Assumptions/Limitations:
 *   None.
 *
 ****************************************************************************/

void meadow_ntpc_start(void)
{
    ntpc_start();
    return;
}
