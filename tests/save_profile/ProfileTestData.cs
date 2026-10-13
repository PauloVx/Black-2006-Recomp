using System;
using System.Security.Cryptography;
using System.Text;

namespace BlackXboxLauncher
{
    // Authored synthetic data only: no retail save bytes or game assets.
    internal static class ProfileTestData
    {
        internal static byte[] Create()
        {
            return Create(new[] { true, true, false }, new[] { "Alpha", "Bravo", "Unused" });
        }

        internal static byte[] Create(bool[] active, string[] names)
        {
            if (active == null || names == null || active.Length != 3 || names.Length != 3)
                throw new ArgumentException("Provide exactly three slot states and names.");
            byte[] data = new byte[0x1800];
            for (int slot = 0; slot < 3; slot++)
            {
                int start = slot * 0x778;
                WriteUInt32(data, start, 0x26);
                WriteUInt32(data, start + 4, 0x778);
                WriteUInt32(data, start + 0xC8, (uint)(100 + slot));
                byte[] name = Encoding.ASCII.GetBytes(names[slot]);
                if (name.Length == 0 || name.Length >= 0x50) throw new ArgumentException("Invalid synthetic slot name.");
                Array.Copy(name, 0, data, start + 0xD0, name.Length);
                // Native empty slots retain their header and old payload, including the name.
                data[start + 0x76D] = active[slot] ? (byte)0 : (byte)1;
                for (int mission = 0; mission < 32; mission++)
                {
                    int record = start + 0x124 + mission * 0x20;
                    WriteUInt32(data, record, 0xFFFFFFFF);
                    data[record + 0x1A] = mission == 0 || mission == 8 || mission == 16 ? (byte)1 : (byte)0;
                }
            }
            UpdateIntegrity(data);
            return data;
        }

        internal static void WriteUInt32(byte[] data, int offset, uint value)
        {
            data[offset] = (byte)value;
            data[offset + 1] = (byte)(value >> 8);
            data[offset + 2] = (byte)(value >> 16);
            data[offset + 3] = (byte)(value >> 24);
        }

        internal static void UpdateIntegrity(byte[] data)
        {
            if (data == null || data.Length != 0x1800) throw new ArgumentException("Invalid synthetic Profile length.");
            for (int slot = 0; slot < 3; slot++)
            {
                int start = slot * 0x778;
                uint sum = 0;
                for (int offset = 0; offset < 0x770; offset += 4)
                    sum = unchecked(sum + BitConverter.ToUInt32(data, start + offset));
                WriteUInt32(data, start + 0x770, sum);
            }
            using (var hmac = new HMACSHA1(new byte[16]))
            {
                byte[] inner = hmac.ComputeHash(data, 0, data.Length - 20);
                byte[] signature = hmac.ComputeHash(inner);
                Array.Copy(signature, 0, data, data.Length - 20, 20);
            }
        }
    }
}
