using UnityEngine;
using System;
using System.IO;
using System.Collections;
using UnityEngine.Networking;
using System.Collections.Generic;

public class TestCall : MonoBehaviour
{
    private bool parallelDecoderTaskRunning = false;
    [SerializeField] private string referenceMeshName = "mesh_frame_reference.obj";
    [SerializeField] private string trajectoriesName = "delta_trajectories_f64.bin";
    [SerializeField] private string bMatrixName = "B_matrix.txt";
    [SerializeField] private string tMatrixName = "T_matrix.txt";
    [SerializeField] private Material sequenceMat;
    private bool decoderValid = false;
    private string currentDecoderName = "";
    private string parallelDecoderName = "";

    string localRef;
        string localTraj;
        string localB;
        string localT;

    private string lastUsedDecoderName = "";
    private MeshFilter meshFilter;
    private int totalFrames;
    private int vertexCount;
    private int[] triangleIndices;
    private int currentFrame = 0;
    private bool readyToSwap;
    private bool sequencePlaying = false;
    private bool nextDecoderLoading = false;
    private string workingDir;
    private Vector3[] vertexArray;
    private Vector3[] baseVertexArray;
    public float playbackFPS = 30f;
    private float playbackTimer = 0f;

    public bool PlayNextSequence = true; // New flag to control looping behavior

    IEnumerator Start()
    {
        workingDir = Path.Combine(Application.persistentDataPath, "TVMDecoder");
        Directory.CreateDirectory(workingDir);
        localRef = Path.Combine(workingDir, referenceMeshName);
        localTraj = Path.Combine(workingDir, trajectoriesName);
        localB = Path.Combine(workingDir, bMatrixName);
        localT = Path.Combine(workingDir, tMatrixName);

        yield return StartCoroutine(VerifyCopy(referenceMeshName, localRef));
        yield return StartCoroutine(VerifyCopy(trajectoriesName, localTraj));
        yield return StartCoroutine(VerifyCopy(bMatrixName, localB));
        yield return StartCoroutine(VerifyCopy(tMatrixName, localT));
        StartCoroutine(PrepareDecoder(true)); // Initial load
    }

    void Update()
    {
        if (!decoderValid || string.IsNullOrEmpty(currentDecoderName) || totalFrames <= 0)
            return;

        if (currentFrame >= totalFrames && readyToSwap && !string.IsNullOrEmpty(parallelDecoderName))
        {
            Debug.Log("[Unity] 🔁 Swapping to new decoder...");
            SwapToParallelDecoder();
            return;
        }

        if (currentFrame >= totalFrames && !readyToSwap)
        {
            Debug.LogWarning("[Unity] ⏳ Waiting for parallel decoder to finish loading...");
            return;
        }

        playbackTimer += Time.deltaTime;
        if (playbackTimer >= 1f / playbackFPS)
        {
            playbackTimer -= 1f / playbackFPS;

            UpdateMeshFromDecoder(currentFrame);
            currentFrame++;
        }
    }



    void SwapToParallelDecoder()
    {
        string oldDecoderName = currentDecoderName;
        currentDecoderName = parallelDecoderName;
        parallelDecoderName = "";

        decoderValid = true;
        currentFrame = 0;
        readyToSwap = false;
        nextDecoderLoading = false;

        totalFrames = TVMDecoderPlugin.GetTotalFrames(currentDecoderName);
        vertexCount = TVMDecoderPlugin.GetVertexCount(currentDecoderName);

        if (meshFilter?.sharedMesh != null)
        {
            meshFilter.sharedMesh.Clear();
        }

        lastUsedDecoderName = oldDecoderName;
        SetupMesh();
        UpdateMeshFromDecoder(0);
        playbackTimer = 0f;
        sequencePlaying = true;

        TVMDecoderPlugin.CleanDecoders(new string[] { currentDecoderName, parallelDecoderName }, 2);
        lastUsedDecoderName = "";
        System.GC.Collect();
        Resources.UnloadUnusedAssets();

        if (PlayNextSequence)
        {
            Debug.Log("[Unity] 🚀 Launching next decoder immediately after swap.");
            StartCoroutine(PrepareNextParallelDecoder());
            nextDecoderLoading = true;
        }
    }

    IEnumerator PrepareDecoder(bool setAsCurrent)
    {
        Debug.Log("[Unity] 🚀 Preparing decoder " + (setAsCurrent ? "(current)" : "(parallel)"));

        string decoderName = $"decoder_{Guid.NewGuid()}";
        if (!TVMDecoderPlugin.CreateDecoder(decoderName, workingDir, true))
        {
            Debug.LogError("[Decoder] ❌ Failed to create decoder.");
            yield break;
        }

        if (!TVMDecoderPlugin.LoadSequence(decoderName, localRef, localTraj, localB, localT))
        {
            Debug.LogError("[Decoder] ❌ LoadSequence failed.");
            TVMDecoderPlugin.CleanDecoders(new string[] { currentDecoderName, parallelDecoderName }, 2);
            yield break;
        }

        Debug.Log("[Decoder] ✅ Decoder ready.");

        if (setAsCurrent)
        {
            StartCoroutine(PrepareNextParallelDecoder());
            currentDecoderName = decoderName;
            decoderValid = true;
            SetupMesh();

            // 🚀 Immediately prepare the next decoder before playback starts
            StartCoroutine(PrepareNextParallelDecoder());
        }
        else
        {
            parallelDecoderName = decoderName;
            readyToSwap = true;
        }
    }

    IEnumerator PrepareNextParallelDecoder()
    {
        if (parallelDecoderTaskRunning)
        {
            Debug.Log("[Unity] ⚠️ Decoder prep already running.");
            yield break;
        }

        parallelDecoderTaskRunning = true;

        while (!string.IsNullOrEmpty(lastUsedDecoderName))
            yield return new WaitForSeconds(0.05f);

        string decoderName = $"decoder_{Guid.NewGuid()}";
        Debug.Log($"[Unity] 🧵 Preparing {decoderName}...");

        string refMeshPath = Path.Combine(workingDir, referenceMeshName);
        string trajPath = Path.Combine(workingDir, trajectoriesName);
        string bMatrixPath = Path.Combine(workingDir, bMatrixName);
        string tMatrixPath = Path.Combine(workingDir, tMatrixName);

        bool loadCompleted = false;
        bool loadSuccess = false;

        System.Threading.Tasks.Task.Run(() =>
        {
            try
            {
                bool created = TVMDecoderPlugin.CreateDecoder(decoderName, workingDir, true);
                bool loaded = created && TVMDecoderPlugin.LoadSequence(decoderName, refMeshPath, trajPath, bMatrixPath, tMatrixPath);

                if (created && !loaded)
                    TVMDecoderPlugin.CleanDecoders(new string[] { currentDecoderName, parallelDecoderName }, 2);

                loadSuccess = created && loaded;
            }
            catch (Exception e)
            {
                Debug.LogError($"[Unity] ❌ Threaded load error: {e.Message}");
            }
            finally { loadCompleted = true; }
        });

        while (!loadCompleted) yield return null;

        if (loadSuccess)
        {
            if (!string.IsNullOrEmpty(parallelDecoderName) && parallelDecoderName != decoderName)
            {
                Debug.LogWarning($"[Unity] 🧹 Cleaning up old parallel: {parallelDecoderName}");
                TVMDecoderPlugin.CleanDecoders(new string[] { currentDecoderName, decoderName }, 2);
            }

            parallelDecoderName = decoderName;
            readyToSwap = true;
            Debug.Log($"[Unity] ✅ Parallel decoder {decoderName} ready.");
        }
        else
        {
            Debug.LogError("[Unity] ❌ Failed to load decoder. Retrying...");
            yield return new WaitForSeconds(1f);
            StartCoroutine(PrepareNextParallelDecoder());
        }

        parallelDecoderTaskRunning = false;
        nextDecoderLoading = false;
    }

    void SetupMesh()
    {
        totalFrames = TVMDecoderPlugin.GetTotalFrames(currentDecoderName);
        vertexCount = TVMDecoderPlugin.GetVertexCount(currentDecoderName);

        float[] refVerts = new float[vertexCount * 3];
        TVMDecoderPlugin.GetReferenceVertices(currentDecoderName, refVerts);

        baseVertexArray = new Vector3[vertexCount];
        for (int i = 0; i < vertexCount; i++)
        {
            int idx = i * 3;
            baseVertexArray[i] = new Vector3(refVerts[idx], refVerts[idx + 1], refVerts[idx + 2]);
        }

        int triangleCount = TVMDecoderPlugin.GetTriangleIndexCount(currentDecoderName);
        triangleIndices = new int[triangleCount];
        TVMDecoderPlugin.GetTriangleIndices(currentDecoderName, triangleIndices, triangleCount);

        vertexArray = new Vector3[vertexCount];

        meshFilter = gameObject.GetComponent<MeshFilter>() ?? gameObject.AddComponent<MeshFilter>();
        MeshRenderer renderer = gameObject.GetComponent<MeshRenderer>() ?? gameObject.AddComponent<MeshRenderer>();
        renderer.material = sequenceMat;

        Mesh mesh = meshFilter.sharedMesh;
        if (mesh == null)
        {
            mesh = new Mesh
            {
                name = "RuntimeNativeMesh",
                indexFormat = UnityEngine.Rendering.IndexFormat.UInt32
            };
            mesh.MarkDynamic();
            meshFilter.sharedMesh = mesh;
        }
        else
        {
            mesh.Clear();
        }

        mesh.vertices = baseVertexArray;
        mesh.triangles = triangleIndices;
        mesh.RecalculateNormals();
        mesh.RecalculateBounds();
    }

    void UpdateMeshFromDecoder(int frame)
    {
        float[] frameData = new float[vertexCount * 3];
        TVMDecoderPlugin.GetFrameDeformedVertices(currentDecoderName, frame, frameData);

        if (frameData.Length < 3 || float.IsNaN(frameData[0]))
        {
            Debug.LogError("[Unity] ❌ Invalid frame data.");
            return;
        }

        for (int i = 0; i < vertexCount; i++)
        {
            int idx = i * 3;
            vertexArray[i] = new Vector3(frameData[idx], frameData[idx + 1], frameData[idx + 2]);
        }

        Mesh mesh = meshFilter.sharedMesh;
        mesh.vertices = vertexArray;
        mesh.RecalculateNormals();
        mesh.RecalculateBounds();

        Debug.Log($"[Unity] ✅ Frame {frame} updated.");
    }

    IEnumerator VerifyCopy(string filename, string destPath)
    {
        string srcPath = Path.Combine(Application.streamingAssetsPath, filename);
        if (File.Exists(destPath)) File.Delete(destPath);

        if (srcPath.Contains("://"))
        {
            using (UnityWebRequest req = UnityWebRequest.Get(srcPath))
            {
                req.downloadHandler = new DownloadHandlerBuffer();
                yield return req.SendWebRequest();
                if (req.result != UnityWebRequest.Result.Success)
                {
                    Debug.LogError($"[Copy] ❌ {filename}: {req.error}");
                    yield break;
                }
                File.WriteAllBytes(destPath, req.downloadHandler.data);
            }
        }
        else
        {
            bool done = false;
            Exception error = null;
            System.Threading.Tasks.Task.Run(() =>
            {
                try { File.Copy(srcPath, destPath, true); }
                catch (Exception e) { error = e; }
                done = true;
            });
            while (!done) yield return null;
            if (error != null)
            {
                Debug.LogError($"[Copy] ❌ {filename}: {error.Message}");
                yield break;
            }
        }

        Debug.Log($"[Copy] ✅ {filename} copied.");
    }

    void OnDestroy()
    {
        TVMDecoderPlugin.CleanDecoders(new string[] { }, 0);
    }
    
    void OnApplicationQuit() {
    TVMDecoderPlugin.CleanDecoders(new string[] { }, 0);
}

void OnDisable() {
    TVMDecoderPlugin.CleanDecoders(new string[] { }, 0);
}
}
