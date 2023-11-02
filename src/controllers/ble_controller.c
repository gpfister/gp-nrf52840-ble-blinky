//
// gp-nrf52840-ble-blinky
// Copyright (c) 2023, Greg PFISTER. MIT Licennse
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "ble_controller.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

#include "led_controller.h"

/* Device name */
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/* Service and characteristics UUID */
#define UUID_ADVERTISED_SERVICE_VAL BT_UUID_128_ENCODE(0xfefb5d60, 0x994e, 0x4d19, 0xbe43, 0xd7849a807198)
#define UUID_BLINKY_SERVICE_VAL BT_UUID_128_ENCODE(0x826c9400, 0x8f2f, 0x4dc5, 0x8319, 0xd07b584cf83)
#define UUID_BLINKY_CHARACTERISTIC_TIME_INTERVAL_VAL BT_UUID_128_ENCODE(0x04df2644, 0xe6b8, 0x4541, 0x8a7f, 0xcecf67f365fe)
#define UUID_BLINKY_CHARACTERISTIC_SEQUENCE_VAL BT_UUID_128_ENCODE(0x7656d6e9, 0x46da, 0x425a, 0x8c9e, 0x4fa2becdf619)
#define UUID_BLINKT_CHARACTERISTSIC_LED_STATUS_VAL BT_UUID_128_ENCODE(0x24b35ad0, 0xd0f0, 0x4811, 0xbdfb, 0x16d4451a514f)

#define UUID_ADVERTISED_SERVICE BT_UUID_DECLARE_128(UUID_ADVERTISED_SERVICE_VAL)
#define UUID_BLINKY_SERVICE BT_UUID_DECLARE_128(UUID_BLINKY_SERVICE_VAL)
#define UUID_BLINKY_CHARACTERISTIC_TIME_INTERVAL BT_UUID_DECLARE_128(UUID_BLINKY_CHARACTERISTIC_TIME_INTERVAL_VAL)
#define UUID_BLINKY_CHARACTERISTIC_SEQUENCE BT_UUID_DECLARE_128(UUID_BLINKY_CHARACTERISTIC_SEQUENCE_VAL)
#define UUID_BLINKT_CHARACTERISTSIC_LED_STATUS BT_UUID_DECLARE_128(UUID_BLINKT_CHARACTERISTSIC_LED_STATUS_VAL)

/* Current connect */
struct bt_conn *current_conn;

/* Advertizing data */
static const struct bt_data advertizing_data[] = {
    // Flags: BLE, no classic
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    // Device name
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* Scanning data */
static const struct bt_data scanning_data[] = {
    // Advertised service
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, UUID_ADVERTISED_SERVICE_VAL),
};

/* Callbacks */
void on_connected(struct bt_conn *conn, uint8_t err);
void on_disconnected(struct bt_conn *conn, uint8_t reason);

struct bt_conn_cb connection_callback = {
    .connected = on_connected,
    .disconnected = on_disconnected,
};

/* Semaphores */
static K_SEM_DEFINE(ble_controller_bt_init_ok, 1, 1);

/* Internal declarations */
void ble_controller_bt_ready(int err);

int ble_controller_init() {
  int ret;

  // Register callbacks
  bt_conn_cb_register(&connection_callback);

  // Unale BLE
  printk("[BLE Controller] Intializing BLE...\n");
  ret = bt_enable(ble_controller_bt_ready);
  if (ret < 0) {
    printk("[BLE Controller] Unable enable Bluetooth (error code %i)\n", ret);
    return -1;
  }

  // Wait for BLE to be initialized
  k_sem_take(&ble_controller_bt_init_ok, K_FOREVER);

  // Advertize
  ret = bt_le_adv_start(BT_LE_ADV_CONN, advertizing_data, ARRAY_SIZE(advertizing_data), scanning_data, ARRAY_SIZE(scanning_data));
  if (ret < 0) {
    printk("[BLE Controller] Unable to start advertizing (error code %i)\n", ret);
    return -2;
  }

  return 0;
}

void ble_controller_bt_ready(int err) {
  if (err) {
    printk("[BLE Controller] BLE initialization failed with return code %i!\n", err);
  } else {
    printk("[BLE Controller] BLE initialization done!\n");
  }

  k_sem_give(&ble_controller_bt_init_ok);
}

void on_connected(struct bt_conn *conn, uint8_t err) {
  if (err) {
    printk("[BLE Controller] Unable to connect (error code %i)\n", err);
    return;
  }

  // Reference current connection
  struct bt_conn *current_conn = bt_conn_ref(conn);
  printk("[BLE Controller] Device connected\n");

  // Set led indicating BLE central is connected
  led_controller_set_bt_connected();
}

void on_disconnected(struct bt_conn *conn, uint8_t reason) {
  printk("[BLE Controller] Device disconnected (reason code %i)\n", reason);

  // If there was an active connect, unreference it
  if (current_conn) {
    bt_conn_unref(current_conn);
    current_conn = NULL;
  }

  // Unset led BLE indicating BLE central is connected
  led_controller_unset_bt_connected();
}