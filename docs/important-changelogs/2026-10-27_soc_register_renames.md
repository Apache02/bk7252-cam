# SoC register renames

Every register and bit-field renamed in `src/platform/soc/bk7221u/include/soc/` during the naming
canonicalisation (see `docs/conventions/soc-register-headers.md`, convention 13). One rename per line:
`hw_<block>-><register>[.<field>] => hw_<block>-><register>[.<field>]`. A register rename and the rename
of a field inside it are separate lines; the field line uses the old register name on the left and the new
one on the right.

```
hw_efuse->ctrl.en => hw_efuse->ctrl.enable
hw_efuse->optr.is_valid => hw_efuse->optr.rd_data_valid

hw_flash->JEDEC_ID => hw_flash->jedec_id
hw_flash->sr_data_crc_cnt => hw_flash->status
hw_flash->conf => hw_flash->config
hw_flash->conf.model_sel => hw_flash->config.mode_sel

hw_gdma->int_counts => hw_gdma->status
hw_gdma->int_status.fin_status => hw_gdma->int_status.fin_int
hw_gdma->int_status.half_fin_status => hw_gdma->int_status.half_fin_int

hw_gpio->func_mux_1 => hw_gpio->func_mode_0_15
hw_gpio->inten => hw_gpio->int_enable
hw_gpio->intlv0 => hw_gpio->int_type_0_15
hw_gpio->intlv1 => hw_gpio->int_type_16_31
hw_gpio->intsta => hw_gpio->int_status
hw_gpio->func_mux_2 => hw_gpio->func_mode_16_31
hw_gpio->func_mux_3 => hw_gpio->func_mode_32_39
hw_gpio->pin_cfg2 => hw_gpio->pin_cfg_32_39
hw_gpio->inten2 => hw_gpio->int_enable_32_39
hw_gpio->intlv3 => hw_gpio->int_type_32_39
hw_gpio->intsta2 => hw_gpio->int_status_32_39

hw_i2c1->config.ensmb => hw_i2c1->config.enable
hw_i2c1->config.sta => hw_i2c1->config.start
hw_i2c1->config.sto => hw_i2c1->config.stop
hw_i2c1->config.ack_tx => hw_i2c1->config.tx_ack
hw_i2c1->config.freq_div => hw_i2c1->config.clk_div
hw_i2c1->config.si => hw_i2c1->config.int_flag
hw_i2c1->config.ack_rx => hw_i2c1->config.rx_ack

hw_i2c2->config.freq_div => hw_i2c2->config.clk_div
hw_i2c2->config.slv_addr => hw_i2c2->config.slave_addr
hw_i2c2->config.smb_cs => hw_i2c2->config.clk_src
hw_i2c2->config.toe => hw_i2c2->config.timeout_en
hw_i2c2->config.ensmb => hw_i2c2->config.enable
hw_i2c2->status.si => hw_i2c2->status.int_flag
hw_i2c2->status.rxfifo_empty => hw_i2c2->status.rx_fifo_empty
hw_i2c2->status.txfifo_full => hw_i2c2->status.tx_fifo_full
hw_i2c2->status.smbus_ack => hw_i2c2->status.ack
hw_i2c2->status.smbus_stop => hw_i2c2->status.stop
hw_i2c2->status.smbus_sta => hw_i2c2->status.start

hw_icu->clk26m_div.div => hw_icu->clk26m_div.clk_div

hw_jpeg->ctrl0.div => hw_jpeg->ctrl0.clk_div
hw_jpeg->byte_cnt_pfrm => hw_jpeg->byte_cnt_per_frame

hw_pwm->ctl => hw_pwm->ctrl

hw_sctrl->control => hw_sctrl->ctrl
hw_sctrl->control.divider => hw_sctrl->ctrl.mclk_div
hw_sctrl->block_enable.temprature_sensor => hw_sctrl->block_enable.temperature_sensor

hw_sdio->config.intr_en_tx => hw_sdio->config.tx_int_en
hw_sdio->config.intr_en_rx => hw_sdio->config.rx_int_en
hw_sdio->config.intr_en_cmd => hw_sdio->config.cmd_int_en
hw_sdio->intr_status => hw_sdio->int_status

hw_spi->ctrl.txint_en => hw_spi->ctrl.tx_fifo_int_en
hw_spi->ctrl.rxint_en => hw_spi->ctrl.rx_fifo_int_en
hw_spi->ctrl.ckr => hw_spi->ctrl.clk_div
hw_spi->ctrl.bit_wdth => hw_spi->ctrl.bit_width
hw_spi->ctrl.ckpol => hw_spi->ctrl.cpol
hw_spi->ctrl.ckpha => hw_spi->ctrl.cpha
hw_spi->ctrl.msten => hw_spi->ctrl.master_en
hw_spi->ctrl.spien => hw_spi->ctrl.enable
hw_spi->stat => hw_spi->int_status
hw_spi->stat.txfifo_empty => hw_spi->int_status.tx_fifo_empty
hw_spi->stat.txfifo_full => hw_spi->int_status.tx_fifo_full
hw_spi->stat.rxfifo_empty => hw_spi->int_status.rx_fifo_empty
hw_spi->stat.rxfifo_full => hw_spi->int_status.rx_fifo_full
hw_spi->stat.txint => hw_spi->int_status.tx_fifo_int
hw_spi->stat.rxint => hw_spi->int_status.rx_fifo_int
hw_spi->stat.spibusy => hw_spi->int_status.busy
hw_spi->dat => hw_spi->data

hw_timer_bank0->ctl => hw_timer_bank0->ctrl
hw_timer_bank0->ctl.clk_divider => hw_timer_bank0->ctrl.clk_div
hw_timer_bank0->ctl.irq_status => hw_timer_bank0->ctrl.int_status
hw_timer_bank1->ctl => hw_timer_bank1->ctrl
hw_timer_bank1->ctl.clk_divider => hw_timer_bank1->ctrl.clk_div
hw_timer_bank1->ctl.irq_status => hw_timer_bank1->ctrl.int_status

hw_uart1->config.clk_divider => hw_uart1->config.clk_div
hw_uart1->fifo_status.tx_full => hw_uart1->fifo_status.tx_fifo_full
hw_uart1->fifo_status.tx_empty => hw_uart1->fifo_status.tx_fifo_empty
hw_uart1->fifo_status.rx_full => hw_uart1->fifo_status.rx_fifo_full
hw_uart1->fifo_status.rx_empty => hw_uart1->fifo_status.rx_fifo_empty
hw_uart1->irq_enable => hw_uart1->int_enable
hw_uart1->irq_status => hw_uart1->int_status
hw_uart2->config.clk_divider => hw_uart2->config.clk_div
hw_uart2->fifo_status.tx_full => hw_uart2->fifo_status.tx_fifo_full
hw_uart2->fifo_status.tx_empty => hw_uart2->fifo_status.tx_fifo_empty
hw_uart2->fifo_status.rx_full => hw_uart2->fifo_status.rx_fifo_full
hw_uart2->fifo_status.rx_empty => hw_uart2->fifo_status.rx_fifo_empty
hw_uart2->irq_enable => hw_uart2->int_enable
hw_uart2->irq_status => hw_uart2->int_status
```

Function renamed together with its field: `sctrl_block_enable_temprature_sensor()` and
`sctrl_block_disable_temprature_sensor()` are now `sctrl_block_enable_temperature_sensor()` and
`sctrl_block_disable_temperature_sensor()`.
