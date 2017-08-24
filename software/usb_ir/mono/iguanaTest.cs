using System;
using IguanaApi;

class Hello
{
    static void Main()
    {
        Console.WriteLine("Hello, World!");

        IntPtr req = IguanaIR.createRequest(0x01, 0, IntPtr.Zero);
        int conn = IguanaIR.connect("0");
        IguanaIR.writeRequest(req, conn);
        IguanaIR.readResponse();
        IguanaIR.close(conn);
    }
}
