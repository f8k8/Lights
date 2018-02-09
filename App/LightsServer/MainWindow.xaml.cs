using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace LightsServer
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();

            // Initialise the list of COM ports
            COMPortDropdown.Items.Clear();
            foreach(var portName in System.IO.Ports.SerialPort.GetPortNames())
            {
                COMPortDropdown.Items.Add(portName);
            }
            if(COMPortDropdown.Items.Contains(LightsServer.Properties.Settings.Default.COMPort))
            {
                COMPortDropdown.SelectedItem = COMPortDropdown.Items.IndexOf(LightsServer.Properties.Settings.Default.COMPort);
            }

            UpdateStartStopButton();
        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            // Save our settings
            LightsServer.Properties.Settings.Default.Save();
        }

        private void StartStopButton_Click(object sender, RoutedEventArgs e)
        {
            App currentApp = (App) Application.Current;
            if(currentApp.IsCapturing())
            {
                currentApp.StopCapturing();
            }
            else
            {
                // Make sure there's a COM port selected
                if (COMPortDropdown.SelectedItem != null)
                {
                    currentApp.StartCapturing((string)COMPortDropdown.SelectedItem);
                }
            }

            UpdateStartStopButton();
        }

        private void UpdateStartStopButton()
        {
            App currentApp = (App)Application.Current;
            if(currentApp.IsCapturing())
            {
                StartStopButton.Content = "Stop";
            }
            else
            {
                StartStopButton.Content = "Start";
            }
        }

        private void DebugButton_Click(object sender, RoutedEventArgs e)
        {
            App currentApp = (App)Application.Current;
            currentApp.RequestBoardDebugInfo();
        }
    }
}
