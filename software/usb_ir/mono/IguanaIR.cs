using System;
using System.Runtime.InteropServices;

namespace IguanaApi
{
    public static class IguanaIR
    {
        const sbyte GETVERSION = 0x01;
        const sbyte RECVON     = 0x12;

        [DllImport ("iguanaIR")]
        private static extern int iguanaConnect_real(string dev, int ver);
        public static int connect(string dev)
        {
            return iguanaConnect_real(dev, 1);
        }

        [DllImport ("iguanaIR")]
        private static extern void iguanaClose(int conn);
        public static void close(int conn)
        {
            iguanaClose(conn);
        }

        [DllImport ("iguanaIR")]
        private static extern IntPtr iguanaCreateRequest(sbyte code, uint dataLength, IntPtr data);
        public static IntPtr createRequest(sbyte code, uint dataLength, IntPtr data)
        {
            return iguanaCreateRequest(code, dataLength, data);
        }

        [DllImport ("iguanaIR")]
        private static extern bool iguanaWriteRequest(IntPtr req, int conn);
        public static bool writeRequest(IntPtr req, int conn)
        {
            return iguanaWriteRequest(req, conn);
        }
    }
}
