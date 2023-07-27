#include "micro_time.h"

#include "../../../../nuttx/include/nuttx/config.h"

#include <cstdint>

namespace tflite 
{

// These functions should be implemented by each target platform, and provide an
// accurate tick count along with how many ticks there are per second.
int32_t ticks_per_second()
{
    return(1e6 / CONFIG_USEC_PER_TICK);
}

// Return time in ticks.  The meaning of a tick varies per platform.
int32_t GetCurrentTimeTicks()
{
    return(0);
}

}