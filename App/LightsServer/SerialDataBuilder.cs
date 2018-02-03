using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace LightsServer
{
    class SerialDataBuilder
    {
        public static byte[] Config(int columnCount, int rowCount)
        {
            // Control character, byte for width, byte for height
            byte[] configData = new byte[3];
            configData[0] = (byte)'C';
            configData[1] = (byte)columnCount;
            configData[2] = (byte)rowCount;
            return configData;
        }

        public static byte[] LightData(int[] lightValues)
        {
            // 3 bytes per light, plus 1 for the control character
            byte[] lightData = new byte[lightValues.Length * 3 + 1];
            lightData[0] = (byte)'L';

            var byteIndex = 1;
            for(var lightIndex = 0; lightIndex < lightValues.Length; ++lightIndex)
            {
                // Extract the R, G and B bytes from the light values
                var lightValue = lightValues[lightIndex];
                byte red = (byte)((lightValue & 0xFF0000) >> 16);
                byte green = (byte)((lightValue & 0xFF00) >> 8);
                byte blue = (byte)((lightValue & 0xFF));

                lightData[byteIndex++] = red;
                lightData[byteIndex++] = green;
                lightData[byteIndex++] = blue;
            }

            return lightData;
        }
    }
}
