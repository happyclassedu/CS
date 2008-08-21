
using System;
using System.Runtime.InteropServices;
using CrystalSpace;

namespace CrystalSpace.InteropServices
{
  [StructLayout(LayoutKind.Sequential),
   CLSCompliant(false)]
  public struct FileBuffer
  {
    public IntPtr buffer;
    public uint readed;
  }

  public class FileIO
  {
    public static long Write(iFile self, byte[] buffer, int offset, long size)
    {
      IntPtr cbuf = NativeCAlloc.Malloc((int)size);

      for(int i=0; i < size;i++)
        Marshal.WriteByte(cbuf, i, buffer[i + offset]);

      uint ret = corePINVOKE.csWriteFile(iFile.getCPtr(self).Handle, cbuf, (uint)size);
      NativeCAlloc.Free(cbuf);
      return (long)ret;
    }

    public static long Read(iFile self, byte[] buffer, int offset, long size)
    {
      FileBuffer ret = corePINVOKE.csReadFile(iFile.getCPtr(self).Handle, (uint)size);

      for(int i=0; i < ret.readed; i++)
        buffer[i + offset] = Marshal.ReadByte(ret.buffer, i);

      NativeCAlloc.Free(ret.buffer);
      return (long)ret.readed;
    }
  };

}; //namespace CrystalSpace.InteropServices

