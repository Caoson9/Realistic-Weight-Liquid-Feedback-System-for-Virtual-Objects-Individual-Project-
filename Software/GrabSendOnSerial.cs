using UnityEngine;
using Oculus.Interaction;          // Grabbable, GrabInteractable
using Oculus.Interaction.HandGrab; // HandGrabInteractable
using System;
using System.Collections;

[DisallowMultipleComponent]
[RequireComponent(typeof(Rigidbody))]
public class GrabSendOnSerial : MonoBehaviour
{
    [Header("Meta XR Components (auto-find)")]
    [SerializeField] private Grabbable _grabbable;
    [SerializeField] private GrabInteractable _grabInteractable;
    [SerializeField] private HandGrabInteractable _handGrabInteractable;

    [Header("Mass in grams")]
    [Tooltip("若 >0 则使用该值；否则直接使用 Rigidbody.mass（按克理解，不做 kg→g 转换）")]
    [SerializeField] private float gramsOverride = 0f;

    [Header("Fallback anchors (optional)")]
    [SerializeField] private Transform leftHandAnchor;
    [SerializeField] private Transform rightHandAnchor;

    private Rigidbody _rb;
    private bool _wasGrabbed = false;
    private float _gramsCached = 0f;

    private void Reset() => AutoFindComponents();

    private void Awake()
    {
        AutoFindComponents();
        _rb = GetComponent<Rigidbody>();
    }

    private void Start()
    {
        // 质量缓存：优先 override，否则直接用 Rigidbody.mass（你已按克填写）
        _gramsCached = (gramsOverride > 0f) ? gramsOverride :
                       (_rb ? Mathf.Max(0f, _rb.mass) : 0f);

        // 自动找手锚点（Oculus rig 常见路径）
        if (!leftHandAnchor) leftHandAnchor = GameObject.Find("LeftHandAnchor")?.transform;
        if (!rightHandAnchor) rightHandAnchor = GameObject.Find("RightHandAnchor")?.transform;
    }

    private void Update()
    {
        bool isGrabbed = IsCurrentlyGrabbed();
        if (isGrabbed != _wasGrabbed)
        {
            int a = ResolveHandedness(); // 0 左 / 1 右
            int b = isGrabbed ? 1 : 0;   // 1 抓 / 0 放
            string msg = $"{a},{b},{_gramsCached:0.00}";
            SerialPortBridge.Instance?.Send(msg);
            Debug.Log($"[GrabSend] {name} -> {msg}");
            _wasGrabbed = isGrabbed;
        }
    }

    private void AutoFindComponents()
    {
        if (_grabbable == null) _grabbable = GetComponentInChildren<Grabbable>(true);
        if (_grabInteractable == null) _grabInteractable = GetComponentInChildren<GrabInteractable>(true);
        if (_handGrabInteractable == null) _handGrabInteractable = GetComponentInChildren<HandGrabInteractable>(true);
    }

    // —— 是否被抓（多 SDK 版本兼容：反射读取 SelectingPointsCount / State） —— //
    private bool IsCurrentlyGrabbed()
    {
        // 1) Grabbable 的 SelectingPointsCount
        if (_grabbable != null)
        {
            try
            {
                var p = typeof(Grabbable).GetProperty("SelectingPointsCount");
                if (p != null) return ((int)p.GetValue(_grabbable, null)) > 0;
            }
            catch { }
        }
        // 2) GrabInteractable.State == Select
        if (_grabInteractable != null)
        {
            try
            {
                var sp = typeof(GrabInteractable).GetProperty("State");
                if (sp != null && sp.GetValue(_grabInteractable, null).ToString().Equals("Select", StringComparison.OrdinalIgnoreCase))
                    return true;
            }
            catch { }
        }
        // 3) HandGrabInteractable.State == Select
        if (_handGrabInteractable != null)
        {
            try
            {
                var sp = typeof(HandGrabInteractable).GetProperty("State");
                if (sp != null && sp.GetValue(_handGrabInteractable, null).ToString().Equals("Select", StringComparison.OrdinalIgnoreCase))
                    return true;
            }
            catch { }
        }
        return false;
    }

    // —— 左右手判断 —— //
    private int ResolveHandedness()
    {
        // A) 从 Grabbable.SelectingInteractors 读取 Handedness
        if (_grabbable != null)
        {
            try
            {
                var intersProp = typeof(Grabbable).GetProperty("SelectingInteractors");
                var listObj = intersProp?.GetValue(_grabbable, null) as System.Collections.IEnumerable;
                if (listObj != null)
                {
                    foreach (var it in listObj)
                    {
                        var handedProp = it.GetType().GetProperty("Handedness") ?? it.GetType().GetProperty("handedness");
                        if (handedProp != null)
                        {
                            var v = handedProp.GetValue(it, null)?.ToString();
                            if (!string.IsNullOrEmpty(v))
                            {
                                var s = v.ToLower();
                                if (s.Contains("left")) return 0;
                                if (s.Contains("right")) return 1;
                            }
                        }
                        // 名称兜底
                        var n = it.ToString().ToLower();
                        if (n.Contains("left")) return 0;
                        if (n.Contains("right")) return 1;
                    }
                }
            }
            catch { }
        }
        // B) 用锚点距离兜底
        if (leftHandAnchor && rightHandAnchor)
        {
            float dl = Vector3.Distance(leftHandAnchor.position, transform.position);
            float dr = Vector3.Distance(rightHandAnchor.position, transform.position);
            return (dl <= dr) ? 0 : 1;
        }
        // C) 实在不行默认左
        return 0;
    }
}
