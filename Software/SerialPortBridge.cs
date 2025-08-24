using System;
using System.IO.Ports;
using UnityEngine;

public class SerialPortBridge : MonoBehaviour
{
    public static SerialPortBridge Instance { get; private set; }

    [Header("Serial")]
    [SerializeField] private string portName = "COM8";
    [SerializeField] private int baudRate = 57600;

    private SerialPort _port;

    private void Awake()
    {
        if (Instance != null && Instance != this) { Destroy(gameObject); return; }
        Instance = this;
        DontDestroyOnLoad(gameObject);

        try
        {
            _port = new SerialPort(portName, baudRate);
            _port.NewLine = "\n";
            _port.DtrEnable = true;
            _port.RtsEnable = false;
            _port.ReadTimeout = 50;
            _port.WriteTimeout = 50;
            _port.Open();
            Debug.Log($"[Serial] Opened {portName}@{baudRate}");
        }
        catch (Exception e)
        {
            Debug.LogError($"[Serial] Open failed: {e.Message}");
        }
    }

    public void Send(string line)
    {
        if (_port == null || !_port.IsOpen) return;
        try { _port.WriteLine(line); }
        catch (Exception e) { Debug.LogWarning($"[Serial] Write failed: {e.Message}"); }
    }

    private void OnApplicationQuit()
    {
        try { if (_port != null && _port.IsOpen) _port.Close(); }
        catch { }
    }
}
