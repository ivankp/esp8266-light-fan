typedef struct {
  const char* const name;
  const int output_pin;
  const int switch_pin;
  const bool inverted;
  volatile bool switch_enable;
  volatile int switch_state;
  TimerHandle_t switch_timer;
} Control;

static Control controls[] = {
  { "light",  5, 13, false, true , 0, NULL },
  { "fan"  ,  4, 14, false, true , 0, NULL },
  { "led"  ,  2, -1, true , false, 0, NULL },
};

// =============================================================================

static void switch_isr(void *arg) {
  Control* const ctrl = controls + (int)arg;
  if (ctrl->switch_enable) {
    ctrl->switch_enable = false;
    // set output pin level
    gpio_set_level(
      ctrl->output_pin,
      (ctrl->switch_state = !ctrl->switch_state)
    );
  }
  // start timer
  BaseType_t xHigherPriorityTaskWoken = pdTRUE;
  xTimerStartFromISR(ctrl->switch_timer, &xHigherPriorityTaskWoken);
}

static void switch_timer_callback(void *arg) {
  Control* const ctrl = controls + (int)arg;
  // set output pin level
  gpio_set_level(ctrl->output_pin,
    (ctrl->switch_state = gpio_get_level(ctrl->switch_pin)));
  // enable switch
  ctrl->switch_enable = true;
  // TODO: notify clients
}

static void init_gpio(void) {
  // Install the GPIO ISR service
  gpio_install_isr_service(0);

  gpio_config_t io_conf = {
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE /* no interrupt */
  };

  FOR_ARRAY(controls, i) {
    Control* const ctrl = controls + i;
    io_conf.pin_bit_mask = (1ull << ctrl->output_pin); // GPIO pin
    gpio_config(&io_conf);
    gpio_set_level(ctrl->output_pin, (int)ctrl->inverted);
  }

  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.intr_type = GPIO_INTR_DISABLE; /* interrupt edge */

  FOR_ARRAY(controls, i) {
    const int switch_pin = controls[i].switch_pin;
    if (switch_pin < 0)
      continue;

    io_conf.pin_bit_mask = (1ull << switch_pin); // GPIO pin
    gpio_config(&io_conf);
  }

  // Add ISR handlers and set interrupt types
  // for gpio pins connected to switches
  FOR_ARRAY(controls, i) {
    const int switch_pin = controls[i].switch_pin;
    if (switch_pin < 0)
      continue;

    controls[i].switch_timer = xTimerCreate/*Static*/(
      "",
      50 / portTICK_PERIOD_MS, // period in ticks
      pdFALSE, // not periodic
      (void*) i, // timer id
      switch_timer_callback
    );
    gpio_isr_handler_add(switch_pin, switch_isr, (void*)i);
    gpio_set_intr_type(switch_pin, GPIO_INTR_ANYEDGE);
  }
}
