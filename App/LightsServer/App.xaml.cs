using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Threading;

namespace LightsServer
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        DispatcherTimer dispatcherTimer;
        System.IO.Ports.SerialPort outputComPort;
        DispatcherTimer serialWriteTimer;
        int lightColumns = 8;
        int lightRows = 8;
        int[] lightValues;

        private System.Windows.Forms.NotifyIcon notifyIcon;

        private void Application_Startup(object sender, StartupEventArgs e)
        {
            // Add the system tray icon
            notifyIcon = new System.Windows.Forms.NotifyIcon();
            notifyIcon.Click += NotifyIcon_Click;
            notifyIcon.DoubleClick += NotifyIcon_DoubleClick;
            notifyIcon.Icon = LightsServer.Properties.Resources.TrayIcon;
            notifyIcon.Visible = true;

            // Have we got a COM port set up?
            if(!String.IsNullOrEmpty(LightsServer.Properties.Settings.Default.COMPort))
            {
                StartCapturing(LightsServer.Properties.Settings.Default.COMPort);
            }
        }
        
        private void Application_Exit(object sender, ExitEventArgs e)
        {
            StopCapturing();
        }

        public void StartCapturing(String comPort)
        {
            StopCapturing();

            // Try and open the output COM port
            outputComPort = new System.IO.Ports.SerialPort(comPort, 115200, System.IO.Ports.Parity.Even, 8, System.IO.Ports.StopBits.One);
            outputComPort.NewLine = "\r\n";
            outputComPort.DataReceived += OutputComPort_DataReceived;
            outputComPort.Open();
            byte[] configData = SerialDataBuilder.Config(lightColumns, lightRows);
            outputComPort.Write(configData, 0, configData.Length);

            lightValues = new int[lightColumns * lightRows];
            if (CaptureProcessor.Start(-1, lightColumns, lightRows))
            {
                // Start the update timer
                dispatcherTimer = new DispatcherTimer();
                dispatcherTimer.Tick += ProcessCapture; ;
                dispatcherTimer.Interval = new TimeSpan(0, 0, 0, 0, 10);
                dispatcherTimer.Start();

                serialWriteTimer = new DispatcherTimer();
                serialWriteTimer.Tick += SendDataToLights;
                serialWriteTimer.Interval = new TimeSpan(0, 0, 0, 1, 0);
                serialWriteTimer.Start();
            }
        }

        private void OutputComPort_DataReceived(object sender, System.IO.Ports.SerialDataReceivedEventArgs e)
        {
            while (outputComPort.BytesToRead > 0)
            {
                System.Diagnostics.Debug.WriteLine(outputComPort.ReadLine());
            }
        }

        private void SendDataToLights(object sender, EventArgs e)
        {
            // Now push the light values to the COM port
            if (outputComPort != null)
            {
                byte[] lightData = SerialDataBuilder.LightData(lightValues);
                outputComPort.Write(lightData, 0, lightData.Length);
                //outputComPort.WriteLine("");
            }
        }

        public void StopCapturing()
        {
            if (dispatcherTimer != null)
            {
                // Stop the update timer
                dispatcherTimer.Stop();
                dispatcherTimer = null;

                // Shutdown capture
                CaptureProcessor.Stop();
            }

            if (serialWriteTimer != null)
            {
                serialWriteTimer.Stop();
                serialWriteTimer = null;
            }

            if (outputComPort != null)
            {
                outputComPort.Dispose();
                outputComPort = null;
            }
        }

        public bool IsCapturing()
        {
            return dispatcherTimer != null;
        }

        private void ProcessCapture(object sender, EventArgs e)
        {
            if(CaptureProcessor.Process())
            {
                // Get the values
                GCHandle handle = GCHandle.Alloc(lightValues, GCHandleType.Pinned);
                try
                {
                    IntPtr pointer = handle.AddrOfPinnedObject();
                    CaptureProcessor.GetLightValues(pointer, lightValues.Length);
                }
                finally
                {
                    if (handle.IsAllocated)
                    {
                        handle.Free();
                    }
                }
            }
        }

        private void NotifyIcon_DoubleClick(object sender, EventArgs e)
        {
            // Show the window
            MainWindow window = new LightsServer.MainWindow();
            window.Show();
        }

        private void NotifyIcon_Click(object sender, EventArgs e)
        {
            
        }
    }
}
