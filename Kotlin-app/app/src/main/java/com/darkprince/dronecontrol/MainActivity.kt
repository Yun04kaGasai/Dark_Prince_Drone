package com.darkprince.dronecontrol

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ListView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatDelegate
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.darkprince.dronecontrol.ble.BleDroneController

class MainActivity : AppCompatActivity(), BleDroneController.Listener {

    companion object {
        private const val PREFS_NAME = "ui_prefs"
        private const val KEY_THEME_MODE = "theme_mode"
        private const val MODE_SYSTEM = 0
        private const val MODE_LIGHT = 1
        private const val MODE_DARK = 2
        private const val THROTTLE_STEP = 15
        private const val THROTTLE_MIN = 0
        private const val THROTTLE_MAX = 255
    }

    private lateinit var bleController: BleDroneController

    private lateinit var tvStatus: TextView
    private lateinit var tvSelectedDevice: TextView
    private lateinit var tvEmptyDevices: TextView
    private lateinit var btnTheme: Button
    private lateinit var etDeviceName: EditText
    private lateinit var sectionDevices: LinearLayout
    private lateinit var sectionControl: LinearLayout
    private lateinit var devicesListView: ListView
    private lateinit var tvThrottle: TextView

    private val motorPins = intArrayOf(4, 5, 2, 3)

    private val discoveredDevices = linkedMapOf<String, String>()
    private lateinit var deviceAdapter: ArrayAdapter<String>
    private var selectedDeviceAddress: String? = null
    private var selectedThemeMode: Int = MODE_SYSTEM
    private var currentThrottle: Int = THROTTLE_MIN

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        val granted = result.values.all { it }
        if (!granted) {
            toast(getString(R.string.permissions_required))
        }
    }

    private val enableBluetoothLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) {
        if (!bleController.isBluetoothEnabled()) {
            toast(getString(R.string.bluetooth_off))
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        applySavedThemeMode()
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        bleController = BleDroneController(this, this)

        tvStatus = findViewById(R.id.tvStatus)
        tvSelectedDevice = findViewById(R.id.tvSelectedDevice)
        tvEmptyDevices = findViewById(R.id.tvEmptyDevices)
        btnTheme = findViewById(R.id.btnTheme)
        etDeviceName = findViewById(R.id.etDeviceName)
        sectionDevices = findViewById(R.id.sectionDevices)
        sectionControl = findViewById(R.id.sectionControl)
        devicesListView = findViewById(R.id.lvDevices)
        tvThrottle = findViewById(R.id.tvThrottle)

        val btnMenuDevices: Button = findViewById(R.id.btnMenuDevices)
        val btnMenuControl: Button = findViewById(R.id.btnMenuControl)
        val btnScan: Button = findViewById(R.id.btnScan)
        val btnConnect: Button = findViewById(R.id.btnConnect)
        val btnDisconnect: Button = findViewById(R.id.btnDisconnect)
        val btnUp: Button = findViewById(R.id.btnUp)
        val btnDown: Button = findViewById(R.id.btnDown)
        val btnArm: Button = findViewById(R.id.btnArm)
        val btnDisarm: Button = findViewById(R.id.btnDisarm)
        val btnStop: Button = findViewById(R.id.btnStop)

        selectedThemeMode = getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
            .getInt(KEY_THEME_MODE, MODE_SYSTEM)
        updateThemeButtonText()

        deviceAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_activated_1, mutableListOf())
        devicesListView.adapter = deviceAdapter
        devicesListView.choiceMode = ListView.CHOICE_MODE_SINGLE

        devicesListView.setOnItemClickListener { _, _, position, _ ->
            val displayText = deviceAdapter.getItem(position) ?: return@setOnItemClickListener
            selectedDeviceAddress = discoveredDevices.entries.firstOrNull {
                formatDeviceDisplay(it.value, it.key) == displayText
            }?.key
            updateSelectedDeviceText()
        }

        btnMenuDevices.setOnClickListener { showDevicesSection() }
        btnMenuControl.setOnClickListener { showControlSection() }

        showDevicesSection()
        updateThrottleText()
        updateSelectedDeviceText()
        updateDevicesEmptyState()

        btnScan.setOnClickListener {
            if (!ensureBleReady()) return@setOnClickListener
            selectedDeviceAddress = null
            discoveredDevices.clear()
            refreshDeviceList()
            updateSelectedDeviceText()
            updateDevicesEmptyState()

            val filter = etDeviceName.text?.toString()?.trim().orEmpty().ifBlank { null }
            bleController.scanForDevices(filter)
            toast(getString(R.string.scan_started))
        }

        btnConnect.setOnClickListener {
            if (!ensureBleReady()) return@setOnClickListener
            val address = selectedDeviceAddress
            if (address == null) {
                toast(getString(R.string.select_device_first))
                return@setOnClickListener
            }
            bleController.connectToDevice(address)
        }

        btnDisconnect.setOnClickListener {
            bleController.disconnect()
            tvStatus.text = getString(R.string.status_disconnected)
        }

        btnUp.setOnClickListener {
            currentThrottle = (currentThrottle + THROTTLE_STEP).coerceIn(THROTTLE_MIN, THROTTLE_MAX)
            updateThrottleText()
            sendThrottleToAllMotors()
        }

        btnDown.setOnClickListener {
            currentThrottle = (currentThrottle - THROTTLE_STEP).coerceIn(THROTTLE_MIN, THROTTLE_MAX)
            updateThrottleText()
            sendThrottleToAllMotors()
        }

        btnArm.setOnClickListener {
            if (!bleController.sendArm()) toast(getString(R.string.arm_not_sent))
        }

        btnDisarm.setOnClickListener {
            if (!bleController.sendDisarm()) toast(getString(R.string.disarm_not_sent))
        }

        btnStop.setOnClickListener {
            if (!bleController.sendStop()) {
                toast(getString(R.string.stop_not_sent))
                return@setOnClickListener
            }
            currentThrottle = THROTTLE_MIN
            updateThrottleText()
        }

        btnTheme.setOnClickListener {
            selectedThemeMode = when (selectedThemeMode) {
                MODE_SYSTEM -> MODE_LIGHT
                MODE_LIGHT -> MODE_DARK
                else -> MODE_SYSTEM
            }
            getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
                .edit()
                .putInt(KEY_THEME_MODE, selectedThemeMode)
                .apply()
            applyThemeMode(selectedThemeMode)
            recreate()
        }
    }

    override fun onDestroy() {
        bleController.disconnect()
        super.onDestroy()
    }

    override fun onStatus(status: String) {
        runOnUiThread {
            tvStatus.text = getString(R.string.status_text, status)
        }
    }

    override fun onError(error: String) {
        runOnUiThread {
            toast(error)
            tvStatus.text = getString(R.string.status_error, error)
        }
    }

    override fun onDeviceFound(deviceName: String, address: String) {
        runOnUiThread {
            if (discoveredDevices.containsKey(address)) {
                return@runOnUiThread
            }
            discoveredDevices[address] = deviceName
            refreshDeviceList()
            updateDevicesEmptyState()
        }
    }

    override fun onConnected() {
        runOnUiThread {
            tvStatus.text = getString(R.string.status_connected)
            toast(getString(R.string.connected_ok))
        }
    }

    override fun onDisconnected() {
        runOnUiThread {
            tvStatus.text = getString(R.string.status_disconnected)
        }
    }

    private fun sendThrottleToAllMotors() {
        for (pin in motorPins) {
            if (!bleController.sendMotorSpeed(pin, currentThrottle)) {
                toast(getString(R.string.motor_not_sent, pin))
                return
            }
        }
    }

    private fun updateThrottleText() {
        tvThrottle.text = getString(R.string.throttle_value, currentThrottle)
    }

    private fun showDevicesSection() {
        sectionDevices.visibility = View.VISIBLE
        sectionControl.visibility = View.GONE
    }

    private fun showControlSection() {
        sectionDevices.visibility = View.GONE
        sectionControl.visibility = View.VISIBLE
    }

    private fun refreshDeviceList() {
        val items = discoveredDevices.entries
            .map { formatDeviceDisplay(it.value, it.key) }
            .sortedBy { it.lowercase() }

        deviceAdapter.clear()
        deviceAdapter.addAll(items)
        deviceAdapter.notifyDataSetChanged()
    }

    private fun updateSelectedDeviceText() {
        val address = selectedDeviceAddress
        if (address == null) {
            tvSelectedDevice.text = getString(R.string.selected_device_none)
            return
        }
        val name = discoveredDevices[address] ?: "Unknown"
        val formatted = formatDeviceDisplay(name, address)
        tvSelectedDevice.text = getString(R.string.selected_device, formatted)
    }

    private fun updateDevicesEmptyState() {
        tvEmptyDevices.visibility = if (discoveredDevices.isEmpty()) View.VISIBLE else View.GONE
    }

    private fun formatDeviceDisplay(name: String, address: String): String = "$name - $address"

    private fun ensureBleReady(): Boolean {
        if (!hasRequiredPermissions()) {
            requestRuntimePermissionsIfNeeded()
            return false
        }
        if (!bleController.isBluetoothEnabled()) {
            val intent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            enableBluetoothLauncher.launch(intent)
            return false
        }
        return true
    }

    private fun hasRequiredPermissions(): Boolean {
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        return permissions.all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun requestRuntimePermissionsIfNeeded() {
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        val missing = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missing.isNotEmpty()) {
            permissionLauncher.launch(missing.toTypedArray())
        }
    }

    private fun applySavedThemeMode() {
        val mode = getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
            .getInt(KEY_THEME_MODE, MODE_SYSTEM)
        applyThemeMode(mode)
    }

    private fun applyThemeMode(mode: Int) {
        when (mode) {
            MODE_LIGHT -> AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO)
            MODE_DARK -> AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES)
            else -> AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM)
        }
    }

    private fun updateThemeButtonText() {
        val label = when (selectedThemeMode) {
            MODE_LIGHT -> getString(R.string.theme_light)
            MODE_DARK -> getString(R.string.theme_dark)
            else -> getString(R.string.theme_system)
        }
        btnTheme.text = getString(R.string.theme_mode, label)
    }

    private fun toast(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }
}
