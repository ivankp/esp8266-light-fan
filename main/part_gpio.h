// TODO: transpose
static const int output_pin[] = { LIGHT_PIN, FAN_PIN };
static volatile int switch_state[] = { 0, 0 };
static volatile bool switch_enable[] = { true, true };
static TimerHandle_t switch_timer[] = { NULL, NULL };

static void switch_isr(void *arg) {
  const int i = (int)arg;
  if (switch_enable[i]) {
    switch_enable[i] = false;
    // set output pin level
    gpio_set_level(output_pin[i], (switch_state[i] = !switch_state[i]));
  }
  // start timer
  BaseType_t xHigherPriorityTaskWoken = pdTRUE;
  xTimerStartFromISR( switch_timer[i], &xHigherPriorityTaskWoken );
}

// TODO: combine timer callbacks
static void light_switch_timer_callback(void *arg) {
  // set output pin level
  gpio_set_level(LIGHT_PIN,
    (switch_state[0] = gpio_get_level(LIGHT_SWITCH_PIN)));
  // enable switch
  switch_enable[0] = true;
  // TODO: notify clients
}

static void fan_switch_timer_callback(void *arg) {
  // set output pin level
  gpio_set_level(FAN_PIN,
    (switch_state[1] = gpio_get_level(FAN_SWITCH_PIN)));
  // enable switch
  switch_enable[1] = true;
  // TODO: notify clients
}

static void init_gpio(void) {
  { gpio_config_t io_conf = {
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE /* no interrupt */
    };

#define OUTPUT_PIN(PIN, VAL) \
    io_conf.pin_bit_mask = (1ull << PIN); /* GPIO pin */ \
    gpio_config(&io_conf); \
    gpio_set_level(PIN, VAL);

    OUTPUT_PIN(  LED_PIN, 1/*inverted*/)
    OUTPUT_PIN(LIGHT_PIN, 0)
    OUTPUT_PIN(  FAN_PIN, 0)

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE; /* interrupt edge */

#define INPUT_PIN(PIN) \
    io_conf.pin_bit_mask = (1ull << PIN); /* GPIO pin */ \
    gpio_config(&io_conf);

    INPUT_PIN(LIGHT_SWITCH_PIN)
    INPUT_PIN(  FAN_SWITCH_PIN)
  }

  switch_timer[0] = xTimerCreate/*Static*/(
    "",
    50 / portTICK_PERIOD_MS, // period in ticks
    pdFALSE, // not periodic
    (void*) 0, // timer id
    light_switch_timer_callback
  );
  switch_timer[1] = xTimerCreate/*Static*/(
    "",
    50 / portTICK_PERIOD_MS, // period in ticks
    pdFALSE, // not periodic
    (void*) 0, // timer id
    fan_switch_timer_callback
  );

  // install gpio isr service
  gpio_install_isr_service(0);

  // hook isr handlers for specific gpio pins
  gpio_isr_handler_add(LIGHT_SWITCH_PIN, switch_isr, (void*)0);
  gpio_isr_handler_add(  FAN_SWITCH_PIN, switch_isr, (void*)1);

  gpio_set_intr_type(LIGHT_SWITCH_PIN, GPIO_INTR_ANYEDGE);
  gpio_set_intr_type(  FAN_SWITCH_PIN, GPIO_INTR_ANYEDGE);
}
