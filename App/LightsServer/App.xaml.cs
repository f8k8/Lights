using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Drawing;
using System.Drawing.Imaging;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace LightsServer
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        // We delay by a bit after we open the serial port, as there seems to be
        // some garbage in a buffer somewhere when we first open, so ignore everything when
        // it first opens
        const int SerialPortOpenDelay = 1 * 1000;

        // Time between keepalives
        const int KeepAliveTime = 1 * 1000;

        System.IO.Ports.SerialPort outputComPort;
        long serialPortOpenDelay;
        long keepaliveTimer;

        DispatcherTimer lightProcessorTimer;
        int lightColumns = 100;
        int lightRows = 3;
        int[] lightValues;
        bool lightsUpdated;
        bool lightDataPending;

        // Set when we've got an active connection to the controller board
        bool boardIsAlive;

        private System.Windows.Forms.NotifyIcon notifyIcon;

        public WriteableBitmap PreviewImage
        {
            get;
            set;
        }

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
            serialPortOpenDelay = DateTime.Now.AddMilliseconds(SerialPortOpenDelay).Ticks;
            outputComPort = new System.IO.Ports.SerialPort(comPort, 288000, System.IO.Ports.Parity.None, 8, System.IO.Ports.StopBits.One);
            outputComPort.NewLine = "\r\n";
            outputComPort.ReadTimeout = 100;
            outputComPort.WriteTimeout = 100;
            outputComPort.DataReceived += OutputComPort_DataReceived;
            outputComPort.Open();
            
            lightValues = new int[lightColumns * lightRows];
            if (CaptureProcessor.Start(-1, lightColumns, lightRows))
            {
                PreviewImage = new WriteableBitmap(lightColumns, lightRows, 72, 72, System.Windows.Media.PixelFormats.Bgr32, null);

                // Start the update timer
                lightProcessorTimer = new DispatcherTimer();
                lightProcessorTimer.Tick += ProcessCapture; ;
                lightProcessorTimer.Interval = new TimeSpan(0, 0, 0, 0, 16);
                lightProcessorTimer.Start();
            }
        }

        private void OutputComPort_DataReceived(object sender, System.IO.Ports.SerialDataReceivedEventArgs e)
        {
            // Ignore any initial input, which is probably garbage from a buffer
            if(serialPortOpenDelay > DateTime.Now.Ticks)
            {
                while(outputComPort.BytesToRead > 0)
                {
                    outputComPort.ReadByte();
                }
                return;
            }

            while (outputComPort.BytesToRead > 0)
            {
                // Read the data
                int readByte = outputComPort.ReadByte();
                switch (readByte)
                {
                    case 'H':
                        {
                            // Board has sent a Hello packet, so send one back
                            boardIsAlive = false;
                            lightDataPending = false;
                            lightsUpdated = false;
                            outputComPort.Write("H");
                        }
                        break;

                    case 'C':
                        {
                            // Board has requested the config, so send it
                            byte[] serialData = SerialDataBuilder.Config(lightColumns, lightRows);
                            outputComPort.Write(serialData, 0, serialData.Length);
                            boardIsAlive = true;
                            keepaliveTimer = DateTime.Now.AddMilliseconds(KeepAliveTime).Ticks;
                        }
                        break;

                    case 'R':
                        {
                            // Board is ready to receive light data
                            if(boardIsAlive && lightValues != null)
                            {
                                byte[] lightData = SerialDataBuilder.LightData(lightValues);
                                outputComPort.Write(lightData, 0, lightData.Length);
                                keepaliveTimer = DateTime.Now.AddMilliseconds(KeepAliveTime).Ticks;
                                lightDataPending = false;
                            }
                        }
                        break;

                    case 'D':
                        {
                            try
                            {
                                System.Diagnostics.Debug.WriteLine(outputComPort.ReadLine());
                            }
                            catch(Exception ex)
                            {
                                
                            }
                            keepaliveTimer = DateTime.Now.AddMilliseconds(KeepAliveTime).Ticks;
                        }
                        break;

                    default:
                        break;
                }
            }
        }
        
        public void StopCapturing()
        {
            if (lightProcessorTimer != null)
            {
                // Stop the update timer
                lightProcessorTimer.Stop();
                lightProcessorTimer = null;

                // Shutdown capture
                CaptureProcessor.Stop();
            }

            if (outputComPort != null)
            {
                outputComPort.Dispose();
                outputComPort = null;
            }
        }

        public bool IsCapturing()
        {
            return lightProcessorTimer != null;
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
                    lightsUpdated = true;
                }
                finally
                {
                    if (handle.IsAllocated)
                    {
                        handle.Free();
                    }
                }

                // Update the preview image
                PreviewImage.Lock();
                unsafe
                {
                    int backbuffer = (int)PreviewImage.BackBuffer;

                    var lightIndex = 0;
                    for(var row = 0; row < lightRows; ++row)
                    {
                        var currentPixel = backbuffer + (row * PreviewImage.BackBufferStride);
                        var direction = 1;

                        // Need to reverse  direction on odd rows to compensate for the reversing
                        // done in the capture library
                        if (row % 2 == 1)
                        {
                            direction = -1;
                            currentPixel += 4 * (lightColumns - 1);
                        }

                        for (var column = 0; column < lightColumns; ++column, ++lightIndex, currentPixel += direction * 4)
                        {
                            *((int*)currentPixel) = lightValues[lightIndex];
                        }
                    }
                }
                Int32Rect dirtyRect = new Int32Rect(0, 0, lightColumns, lightRows);
                PreviewImage.AddDirtyRect(dirtyRect);
                PreviewImage.Unlock();
            }

            if (boardIsAlive && outputComPort != null)
            {
                // If we've got updated light values, and an active board to send to, notify the board
                if (!lightDataPending)
                {
                    if (lightsUpdated)
                    {
                        // Notify the board we have updated lights data
                        outputComPort.Write("A");
                        keepaliveTimer = DateTime.Now.AddMilliseconds(KeepAliveTime).Ticks;
                        lightDataPending = true;
                        lightsUpdated = false;
                    }
                    else
                    {
                        // Just send our keepalive
                        if(keepaliveTimer < DateTime.Now.Ticks)
                        {
                            keepaliveTimer = DateTime.Now.AddMilliseconds(KeepAliveTime).Ticks;
                            System.Diagnostics.Debug.WriteLine("Sending keepalive");
                            outputComPort.Write("K");
                        }
                    }
                }
            }

            // If we've stopped capturing (because of an error), then make sure to stop the updates
            if(!CaptureProcessor.IsRunning())
            {
                StopCapturing();
            }
        }

        internal void RequestBoardDebugInfo()
        {
            if(outputComPort != null && keepaliveTimer < DateTime.Now.Ticks)
            {
                outputComPort.Write("D");
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
