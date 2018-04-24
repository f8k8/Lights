using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace LightsServer
{
    class CaptureProcessor
    {
        [DllImport("CaptureProcessor.dll")]
        public static extern bool Start(int singleOutput, int lightColumns, int lightRows);

        [DllImport("CaptureProcessor.dll")]
        public static extern bool Process();

        [DllImport("CaptureProcessor.dll")]
        public static extern bool IsRunning();

        [DllImport("CaptureProcessor.dll")]
        public static extern void GetLightValues(IntPtr values, int length);

        [DllImport("CaptureProcessor.dll")]
        public static extern void Stop();
    }
}
