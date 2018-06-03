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

            // Set up the tint
            RedTint.Value = LightsServer.Properties.Settings.Default.RedTint;
            GreenTint.Value = LightsServer.Properties.Settings.Default.GreenTint;
            BlueTint.Value = LightsServer.Properties.Settings.Default.BlueTint;

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
                    LightsServer.Properties.Settings.Default.COMPort = (string)COMPortDropdown.SelectedItem;

                    PreviewImage.Source = currentApp.PreviewImage;
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

        private void RedTint_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            LightsServer.Properties.Settings.Default.RedTint = (float)e.NewValue;
        }

        private void GreenTint_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            LightsServer.Properties.Settings.Default.GreenTint = (float)e.NewValue;
        }

        private void BlueTint_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            LightsServer.Properties.Settings.Default.BlueTint = (float)e.NewValue;
        }
    }
}
