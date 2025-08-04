using System;
using System.Runtime.InteropServices;
using UnityEngine;

public static class TVMDecoderPlugin
{
#if UNITY_STANDALONE_WIN
    private const string LIB_NAME = "TVMDecoder";         // TVMDecoder.dll
#elif UNITY_STANDALONE_OSX
    private const string LIB_NAME = "TVMDecoder";         // libTVMDecoder.dylib
#elif UNITY_STANDALONE_LINUX
    private const string LIB_NAME = "TVMDecoder";         // libTVMDecoder.so
#elif UNITY_ANDROID
    private const string LIB_NAME = "TVMDecoder";         // libTVMDecoder.so
#else
    private const string LIB_NAME = "__Internal";         // iOS or fallback
#endif

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern bool CreateDecoder(string name, string outputPath, bool logging);

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern bool LoadSequence(string name, string mesh, string dHat, string b, string t);

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetTriangleIndexCount(string name);

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void GetTriangleIndices(string name, int[] outIndices, int maxCount);

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetTotalFrames(string name);

    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern int GetVertexCount(string name);


    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void GetReferenceVertices(string name, float[] outVertices);
    
    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    
    public static extern void GetFrameDeformedVertices(string name, int frameIndex, float[] outVertices);


    [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
    public static extern void CleanDecoders([In] string[] protectedNames, int count);

}
