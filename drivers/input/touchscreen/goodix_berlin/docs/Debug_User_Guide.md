# **Debug User Guide**
## **Introduce**
The driver provides some sysfs and procfs node to debug. You can use them after driver initialize successfully.

## **sysfs node**
It's located under ``/sys/devices/platform/goodix_ts.0``.

* **chip_info**

    show firmware version, config id, sensor id and so on.

    example:
    ```shell
    cat /sys/devices/platform/goodix_ts.0/chip_info
    rom_pid:BERLIN
    rom_vid:020004
    patch_pid:9895
    patch_vid:00000005
    sensorid:255
    config_id:614aaa7d
    config_version:0
    ```

* **get_mcu_fab**

    show fabs of the MCU DIE.(only support Berlin-d,Nottingham currently)

    example:
    ```shell
    cat /sys/devices/platform/goodix_ts.0/get_mcu_fab
    0xFF    # means TSMC
    0x02    # means HLMC
    ```

* **driver_info**

    show driver version.

    example:
    ```shell
    cat /sys/devices/platform/goodix_ts.0/driver_info
    DriverVersion:v1.2.6
    ```

* **irq_info**

    show irq count or enable/disable irq.

    example:
    ```shell
    cat /sys/devices/platform/goodix_ts.0/irq_info
    irq:107
    state:enabled
    disable-depth:0
    trigger-count:82

    echo 0 > /sys/devices/platform/goodix_ts.0/irq_info # disable irq
    echo 1 > /sys/devices/platform/goodix_ts.0/irq_info # enable irq
    ```

* **esd_info**

    enable/disable esd.

    example:
    ```shell
    echo 0 > /sys/devices/platform/goodix_ts.0/esd_info # disable esd
    echo 1 > /sys/devices/platform/goodix_ts.0/esd_info # enable esd
    ```

* **debug_log**

    enable/disable debug level log.

    example:
    ```shell
    echo 0 > /sys/devices/platform/goodix_ts.0/debug_log # close
    echo 1 > /sys/devices/platform/goodix_ts.0/debug_log # open
    ```

* **read_cfg**

    read 200 bytes config from IC.

    example:
    ```shell
    cat /sys/devices/platform/goodix_ts.0/read_cfg
    46,50,43,42,00,00,00,00,39,38,39,35,00,00,00,00,00,00,00,00,
    43,4f,4d,00,00,00,00,00,00,00,7d,aa,4a,61,00,32,30,32,31,30,
    39,32,32,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,80,
    04,3a,fb,06,3a,01,11,22,00,01,03,04,02,07,06,05,0a,09,08,0b,
    0c,0d,0f,0e,10,ff,1a,10,04,0f,15,06,20,07,09,1f,0d,03,01,02,
    00,05,14,08,1b,0e,0a,17,0b,0c,1d,12,11,21,16,13,1c,18,1e,19,
    ff,ff,0e,02,91,2a,00,54,64,00,20,08,02,00,00,00,0c,03,2c,01,
    20,08,04,32,02,00,00,00,0e,04,08,00,00,00,00,00,00,00,d0,07,
    00,00,1a,08,38,04,60,09,50,00,64,00,1e,00,28,00,3c,00,50,00,
    46,00,46,00,0a,0a,00,00,04,0a,04,10,02,0b,30,0c,02,02,00,02,
    ```

* **send_cfg**

    send config to IC. Need ``goodix_cfg_group.cfg``.

    example:
    ```shell
    echo 1 > /sys/devices/platform/goodix_ts.0/send_cfg
    ```

* **reset**

    reset IC manually.

    example:
    ```shell
    echo 1 > /sys/devices/platform/goodix_ts.0/reset
    ```

* **reg_rw**

    read/write data to IC manually.

    example:
    ```shell
    # write 4 bytes(01 02 03 04) to 0x10338
    echo w:10338:4:01:02:03:04 > /sys/devices/platform/goodix_ts.0/reg_rw

    # read 4 bytes from 0x10338
    echo r:10338:4 > /sys/devices/platform/goodix_ts.0/reg_rw
    cat /sys/devices/platform/goodix_ts.0/reg_rw
    0x10338,4 {01 02 03 04}
    ```

* **get_rawdata**

    open/short test. Need ``goodix_test_limits_{sensor_id}.csv``.

    example:
    ```shell
    cat /sys/devices/platform/goodix_ts.0/get_rawdata
    resultInfo: [PASS]-0P-1P-2P-3P-5P-7P-[914,986,785][0,2,0][1,8,0]-GT9895
    ```

* **fwupdate**

    force update manually.

    example:
    ```shell
    # update from goodix_firmware.bin
    echo 2 > /sys/devices/platform/goodix_ts.0/fwupdate/update_en
    cat /sys/devices/platform/goodix_ts.0/fwupdate/result
    result:success  spend_time:5732ms

    # update from special binary
    cat xxx.bin > /sys/devices/platform/goodix_ts.0/fwupdate/fwimage
    echo 1 > /sys/devices/platform/goodix_ts.0/fwupdate/update_en
    ```

* **gesture**

    enable/disable gesture(double tap, single tap, fod)

    example:
    ```shell
    # enable double tap gesture
    echo 1 > /sys/devices/platform/goodix_ts.0/gesture/double_en
    cat /sys/devices/platform/goodix_ts.0/gesture/double_en
    enable

    # disable single tap gesture
    echo 0 > /sys/devices/platform/goodix_ts.0/gesture/single_en
    cat /sys/devices/platform/goodix_ts.0/gesture/single_en
    disable
    ```

## **sysfs node**
It's located under ``proc/goodix_ts``.

* **tp_capacitance_data**

    show a frame of mutual rawdata and diffdata.

    example:
    ```shell
    cat proc/goodix_ts/tp_capacitance_data
    TX:17  RX:34
    mutual_rawdata:
    850,  899,  883,  893,  904,  913,  918,  908,  910,  909,  924,  906,  921,  915,  920,  940,  913,
    861,  892,  872,  884,  897,  907,  914,  911,  917,  922,  933,  907,  922,  915,  913,  927,  908,
    864,  899,  872,  887,  908,  931,  938,  934,  937,  950,  956,  930,  939,  936,  922,  934,  910,
    850,  884,  860,  876,  892,  922,  925,  930,  927,  938,  947,  923,  934,  930,  912,  916,  889,
    852,  890,  871,  884,  908,  930,  934,  928,  930,  941,  949,  928,  938,  931,  912,  917,  893,
    851,  890,  872,  885,  905,  934,  931,  927,  928,  938,  947,  924,  937,  927,  912,  916,  891,
    876,  911,  897,  910,  928,  942,  950,  941,  942,  960,  966,  943,  958,  951,  938,  934,  905,
    863,  901,  889,  903,  914,  933,  930,  931,  932,  943,  952,  925,  938,  935,  924,  923,  901,
    865,  907,  893,  906,  913,  935,  934,  929,  933,  944,  949,  927,  937,  933,  923,  923,  899,
    882,  928,  913,  920,  926,  944,  943,  941,  943,  957,  967,  940,  955,  948,  935,  935,  913,
    867,  912,  897,  903,  906,  925,  929,  923,  925,  937,  947,  923,  935,  927,  919,  920,  897,
    878,  919,  905,  912,  915,  936,  933,  930,  932,  945,  951,  930,  942,  938,  925,  927,  903,
    871,  917,  901,  916,  914,  933,  930,  928,  927,  944,  954,  931,  940,  935,  922,  916,  893,
    858,  905,  889,  899,  902,  919,  920,  915,  920,  932,  941,  920,  930,  924,  905,  903,  882,
    872,  918,  910,  917,  923,  940,  939,  933,  936,  947,  958,  940,  952,  946,  928,  930,  909,
    847,  896,  884,  891,  896,  910,  913,  905,  910,  923,  935,  914,  927,  917,  901,  900,  880,
    859,  907,  896,  905,  903,  919,  921,  916,  922,  938,  950,  942,  954,  933,  918,  915,  890,
    880,  928,  910,  919,  918,  938,  937,  933,  936,  952,  974,  955,  985,  956,  935,  931,  905,
    872,  914,  897,  903,  904,  916,  919,  914,  918,  934,  945,  923,  944,  956,  929,  921,  893,
    877,  920,  903,  909,  906,  917,  923,  916,  919,  933,  943,  919,  936,  958,  928,  921,  896,
    869,  911,  893,  898,  895,  908,  916,  910,  913,  931,  936,  915,  929,  936,  930,  918,  888,
    885,  933,  916,  923,  920,  932,  938,  929,  932,  951,  959,  935,  951,  946,  936,  934,  907,
    871,  918,  902,  907,  906,  917,  926,  920,  921,  936,  948,  923,  932,  931,  916,  915,  898,
    870,  912,  901,  906,  902,  914,  921,  914,  919,  934,  944,  918,  928,  923,  906,  903,  880,
    881,  917,  910,  912,  919,  919,  927,  924,  930,  945,  953,  926,  940,  930,  915,  912,  889,
    888,  924,  916,  921,  919,  932,  938,  932,  939,  954,  966,  942,  954,  939,  923,  917,  900,
    885,  925,  914,  926,  919,  928,  937,  931,  934,  956,  962,  937,  946,  936,  917,  915,  898,
    874,  910,  899,  909,  901,  914,  920,  910,  918,  933,  948,  917,  925,  908,  889,  892,  875,
    863,  899,  891,  901,  901,  909,  915,  905,  909,  923,  934,  908,  908,  894,  872,  877,  867,
    885,  926,  916,  931,  931,  935,  942,  928,  929,  947,  957,  934,  937,  913,  897,  900,  893,
    865,  897,  889,  906,  910,  911,  920,  905,  908,  922,  934,  905,  907,  889,  875,  883,  876,
    868,  903,  884,  905,  908,  910,  920,  905,  903,  918,  934,  910,  910,  894,  874,  879,  875,
    854,  887,  866,  880,  887,  892,  899,  884,  884,  901,  918,  895,  904,  887,  871,  874,  865,
    782,  821,  809,  823,  827,  828,  838,  826,  826,  836,  851,  831,  842,  843,  821,  822,  800,
    mutual_diffdata:
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    ```


