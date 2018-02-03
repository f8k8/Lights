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
        public static extern bool Start();

        [DllImport("CaptureProcessor.dll")]
        public static extern void Process();

        [DllImport("CaptureProcessor.dll")]
        public static extern void Stop();
    }
}
