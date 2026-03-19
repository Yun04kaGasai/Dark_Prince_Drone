package com.darkprince.dronecontrol.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import java.util.UUID

class BleDroneController(
    context: Context,
    private val listener: Listener
) {
    interface Listener {
        fun onStatus(status: String)
        fun onError(error: String)
        fun onDeviceFound(deviceName: String, address: String)
        fun onConnected()
        fun onDisconnected()
    }

    companion object {
        val SERVICE_UUID: UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
        val COMMAND_CHAR_UUID: UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
        private const val DEFAULT_SCAN_MS = 8_000L
    }

    private val appContext = context.applicationContext
    private val bluetoothManager = appContext.getSystemService(BluetoothManager::class.java)
    private val adapter: BluetoothAdapter? = bluetoothManager?.adapter
    private val scanner: BluetoothLeScanner?
        get() = adapter?.bluetoothLeScanner

    private val mainHandler = Handler(Looper.getMainLooper())

    private var isScanning = false
    private var gatt: BluetoothGatt? = null
    private var commandCharacteristic: BluetoothGattCharacteristic? = null
    private var scanFilterName: String? = null

    private val scanStopRunnable = Runnable {
        if (isScanning) {
            stopScanInternal()
            listener.onStatus("Сканирование завершено")
        }
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device ?: return
            val name = device.name?.takeIf { it.isNotBlank() } ?: "Unknown"
            val filter = scanFilterName
            if (filter != null && !name.contains(filter, ignoreCase = true)) {
                return
            }
            listener.onDeviceFound(name, device.address)
        }

        override fun onScanFailed(errorCode: Int) {
            stopScanInternal()
            listener.onError("Ошибка BLE сканирования: $errorCode")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothGatt.STATE_CONNECTED -> {
                    this@BleDroneController.gatt = gatt
                    listener.onStatus("Подключено, поиск сервисов...")
                    gatt.discoverServices()
                }
                BluetoothGatt.STATE_DISCONNECTED -> {
                    listener.onStatus("Отключено")
                    commandCharacteristic = null
                    this@BleDroneController.gatt?.close()
                    this@BleDroneController.gatt = null
                    listener.onDisconnected()
                }
                else -> Unit
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                listener.onError("Не удалось прочитать сервисы: $status")
                return
            }
            val service: BluetoothGattService? = gatt.getService(SERVICE_UUID)
            val characteristic = service?.getCharacteristic(COMMAND_CHAR_UUID)
            if (characteristic == null) {
                listener.onError("Командная характеристика не найдена")
                return
            }
            commandCharacteristic = characteristic
            listener.onStatus("Готово к отправке команд")
            listener.onConnected()
        }
    }

    fun isBluetoothEnabled(): Boolean = adapter?.isEnabled == true

    @SuppressLint("MissingPermission")
    fun scanForDevices(nameFilter: String?) {
        if (isScanning) return
        val bleScanner = scanner
        if (bleScanner == null) {
            listener.onError("BLE сканер недоступен")
            return
        }

        scanFilterName = nameFilter?.trim()?.takeIf { it.isNotEmpty() }
        isScanning = true
        listener.onStatus("Сканирование BLE...")
        bleScanner.startScan(scanCallback)
        mainHandler.postDelayed(scanStopRunnable, DEFAULT_SCAN_MS)
    }

    @SuppressLint("MissingPermission")
    fun connectToDevice(address: String) {
        val btAdapter = adapter
        if (btAdapter == null) {
            listener.onError("Bluetooth адаптер недоступен")
            return
        }

        val device = try {
            btAdapter.getRemoteDevice(address)
        } catch (_: IllegalArgumentException) {
            listener.onError("Неверный адрес устройства")
            return
        }

        disconnect()
        listener.onStatus("Подключение к ${device.name ?: device.address}...")
        gatt = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            device.connectGatt(appContext, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } else {
            device.connectGatt(appContext, false, gattCallback)
        }
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        mainHandler.removeCallbacks(scanStopRunnable)
        stopScanInternal()
        commandCharacteristic = null
        gatt?.disconnect()
        gatt?.close()
        gatt = null
    }

    @SuppressLint("MissingPermission")
    fun sendArm(): Boolean = sendCommand("ARM")

    @SuppressLint("MissingPermission")
    fun sendDisarm(): Boolean = sendCommand("DISARM")

    @SuppressLint("MissingPermission")
    fun sendStop(): Boolean = sendCommand("STOP")

    @SuppressLint("MissingPermission")
    fun sendMotorSpeed(pin: Int, speed: Int): Boolean {
        val safeSpeed = speed.coerceIn(0, 255)
        return sendCommand("M,$pin,$safeSpeed")
    }

    @SuppressLint("MissingPermission")
    private fun sendCommand(command: String): Boolean {
        val btGatt = gatt
        val characteristic = commandCharacteristic
        if (btGatt == null || characteristic == null) {
            listener.onError("Нет BLE соединения")
            return false
        }

        val payload = "$command\n".toByteArray(Charsets.UTF_8)
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            btGatt.writeCharacteristic(
                characteristic,
                payload,
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            ) == BluetoothGatt.GATT_SUCCESS
        } else {
            characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            characteristic.value = payload
            btGatt.writeCharacteristic(characteristic)
        }
    }

    @SuppressLint("MissingPermission")
    private fun stopScanInternal() {
        if (!isScanning) return
        isScanning = false
        mainHandler.removeCallbacks(scanStopRunnable)
        scanner?.stopScan(scanCallback)
    }
}
