import onnxruntime as ort
import numpy as np
import cv2
import os

MODEL_PATH = "FasterRCNN-12.onnx"
IMAGE_PATH = "demo.jpg"
OUTPUT_PATH = "result_fixed.jpg"
SCORE_THRESH = 0.5

COCO_NAMES = [
    '__background__', 'person', 'bicycle', 'car', 'motorcycle', 'airplane', 'bus',
    'train', 'truck', 'boat', 'traffic light', 'fire hydrant', 'N/A', 'stop sign',
    'parking meter', 'bench', 'bird', 'cat', 'dog', 'horse', 'sheep', 'cow',
    'elephant', 'bear', 'zebra', 'giraffe', 'N/A', 'backpack', 'umbrella', 'N/A', 'N/A',
    'handbag', 'tie', 'suitcase', 'frisbee', 'skis', 'snowboard', 'sports ball',
    'kite', 'baseball bat', 'baseball glove', 'skateboard', 'surfboard', 'tennis racket',
    'bottle', 'N/A', 'wine glass', 'cup', 'fork', 'knife', 'spoon', 'bowl',
    'banana', 'apple', 'sandwich', 'orange', 'broccoli', 'carrot', 'hot dog', 'pizza',
    'donut', 'cake', 'chair', 'couch', 'potted plant', 'bed', 'N/A', 'dining table',
    'N/A', 'N/A', 'toilet', 'N/A', 'tv', 'laptop', 'mouse', 'remote', 'keyboard', 'cell phone',
    'microwave', 'oven', 'toaster', 'sink', 'refrigerator', 'N/A', 'book',
    'clock', 'vase', 'scissors', 'teddy bear', 'hair drier', 'toothbrush'
]


def preprocess_raw(img_path):
    """
    直接读取原图，转 RGB，转 float32。
    不做 resize，不做 normalize，让模型内部处理。
    输入 shape: [3, H, W]
    """
    img = cv2.imread(img_path)
    if img is None:
        raise FileNotFoundError(f"无法读取: {img_path}")
    
    h, w = img.shape[:2]
    print(f"原图尺寸: {w}x{h} (W x H)")
    
    # BGR -> RGB
    img_rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    
    # 转 float32（ONNX 通常不接受 uint8，即使模型内部会归一化）
    # 如果下面报错，改成 .astype(np.uint8) 试试
    img_chw = np.transpose(img_rgb, (2, 0, 1)).astype(np.float32)
    
    return img_chw, img  # 返回原图用于可视化


def inference(model_path, input_np):
    session = ort.InferenceSession(model_path, providers=['CPUExecutionProvider'])
    
    inp = session.get_inputs()[0]
    print(f"Input : {inp.name}, shape={inp.shape}, type={inp.type}")
    for i, out in enumerate(session.get_outputs()):
        print(f"Output{i}: {out.name}, shape={out.shape}, type={out.type}")
    
    outputs = session.run(None, {inp.name: input_np})
    return outputs[0], outputs[1], outputs[2]


def draw_detections(image, boxes, labels, scores, score_thresh=0.5):
    vis = image.copy()
    colors = np.random.randint(0, 255, (91, 3), dtype=np.uint8)
    
    count = 0
    h, w = vis.shape[:2]
    
    for i in range(len(boxes)):
        s = scores[i]
        if s < score_thresh:
            continue
        
        box = boxes[i]
        label_id = int(labels[i])
        
        # 模型输出的坐标已经是原图坐标，直接画
        x1, y1, x2, y2 = map(int, map(float, box))
        
        # 限制在图像边界内
        x1, y1 = max(0, x1), max(0, y1)
        x2, y2 = min(w - 1, x2), min(h - 1, y2)
        if x2 <= x1 or y2 <= y1:
            continue
        
        color = tuple(int(c) for c in colors[label_id % len(colors)])
        name = COCO_NAMES[label_id] if label_id < len(COCO_NAMES) else f"id_{label_id}"
        text = f"{name}: {s:.2f}"
        
        cv2.rectangle(vis, (x1, y1), (x2, y2), color, 2)
        (tw, th), _ = cv2.getTextSize(text, cv2.FONT_HERSHEY_SIMPLEX, 0.6, 1)
        cv2.rectangle(vis, (x1, y1 - th - 10), (x1 + tw, y1), color, -1)
        cv2.putText(vis, text, (x1, y1 - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
        
        print(f"  [{count}] {name}: score={s:.3f}, box=({x1},{y1},{x2},{y2})")
        count += 1
    
    return vis, count


def main():
    print("=" * 60)
    print("Faster R-CNN 修正版：直接传入原图")
    print("=" * 60)
    
    if not os.path.exists(MODEL_PATH) or not os.path.exists(IMAGE_PATH):
        print("请修改 MODEL_PATH 和 IMAGE_PATH")
        return
    
    # 1. 预处理：只转 RGB，不做 resize
    print("\n[1/3] 预处理：读取原图，转 RGB")
    input_np, orig_img = preprocess_raw(IMAGE_PATH)
    print(f"      输入 tensor: shape={input_np.shape}, dtype={input_np.dtype}")
    
    # 2. 推理
    print(f"\n[2/3] 推理")
    boxes, labels, scores = inference(MODEL_PATH, input_np)
    print(f"      原始输出框数: {len(boxes)}")
    
    # 3. 过滤 + 可视化
    print(f"\n[3/3] 过滤 (阈值 >= {SCORE_THRESH})")
    mask = scores >= SCORE_THRESH
    boxes, labels, scores = boxes[mask], labels[mask], scores[mask]
    
    vis_img, num = draw_detections(orig_img, boxes, labels, scores, SCORE_THRESH)
    cv2.imwrite(OUTPUT_PATH, vis_img)
    
    print(f"\n{'=' * 60}")
    print(f"完成！检测到 {num} 个物体，结果保存至: {OUTPUT_PATH}")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
