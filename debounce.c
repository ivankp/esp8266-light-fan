static const uint16_t // [T] = ms
  jitter_time = 5,
  multiclick_time = 100;

static volatile uint8_t count = 0;

// 0   1  2   3  4  5  6  7  8
//    ┌──┐   ┌──┐  ┌──┐  ┌──┐
//    │  │   │  │  │  │  │  │
// ───┘  └───┘  └──┘  └──┘  └───

static void button_isr(void *arg) { // trigger on both edges
  // count % 2 == 0  :  trigger on  rising, untrigger on falling
  // count % 2 == 1  :  trigger on falling, untrigger on rising

  // edge == 0  :  isr triggered on falling
  // edge == 1  :  isr triggered on rising

  if ((count%2) != edge) {
    // start jitter timer
  } else {
    // stop jitter timer
  }
}

static void jitter_timer_callback(void *arg) {
  if ((++count)%2) { // rising edge
    // start multiclick timer
  }
}
static void multiclick_timer_callback(void *arg) {
  const uint8_t n = (count+1) >> 1; // number of rising edges
  count = -(count%2); // ignore upcoming falling edge
  if (n%2) { // single click
  } else { // double click
  }
}
