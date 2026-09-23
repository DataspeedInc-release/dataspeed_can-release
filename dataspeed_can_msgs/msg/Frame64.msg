# Extension of dataspeed_can_msgs/Frame with fixed data array size to prevent memory overrun in PlotJuggler

std_msgs/Header header

uint32 id
bool extended
bool fdf
bool brs
bool esi
bool rtr
uint8 size
uint8[64] data
