using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
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
            outputComPort = new System.IO.Ports.SerialPort(comPort, 115200, System.IO.Ports.Parity.None, 8, System.IO.Ports.StopBits.One);
            outputComPort.Open();

            if (CaptureProcessor.Start())
            {
                // Start the update timer
                dispatcherTimer = new DispatcherTimer();
                dispatcherTimer.Tick += ProcessCapture; ;
                dispatcherTimer.Interval = new TimeSpan(0, 0, 0, 0, 10);
                dispatcherTimer.Start();
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

            if(outputComPort != null)
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
            CaptureProcessor.Process();
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
