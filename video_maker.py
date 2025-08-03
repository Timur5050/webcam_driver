#!/usr/bin/env python3
import cv2
import glob
import os

input_folder = "/home/timur/test"
output_video = "webcam.mp4"
fps = 15

jpeg_files = sorted(glob.glob(f"{input_folder}/frame_*.jpg"))

if not jpeg_files:
    print("Немає JPEG файлів!")
    exit()

print(f"Знайдено {len(jpeg_files)} кадрів")

first_frame = cv2.imread(jpeg_files[0])
if first_frame is None:
    print("Не вдалося прочитати перший кадр!")
    exit()

height, width = first_frame.shape[:2]
print(f"Розмір відео: {width}x{height}")

codecs_to_try = [
    ('mp4v', 'MP4V'),  # поточний кодек
    ('avc1', 'H264'),  # H.264 кодек
    ('X264', 'H264'),  # Альтернативний H.264
    ('XVID', 'XVID')   # Резервний варіант
]

video = None
for fourcc_str, codec_name in codecs_to_try:
    try:
        fourcc = cv2.VideoWriter_fourcc(*fourcc_str)
        video = cv2.VideoWriter(output_video, fourcc, fps, (width, height))
        
        if video.isOpened():
            print(f"Використовується кодек: {codec_name}")
            break
        else:
            video.release()
            video = None
    except Exception as e:
        print(f"Кодек {codec_name} не працює: {e}")
        if video:
            video.release()
            video = None

if video is None:
    print("Не вдалося створити відео з жодним кодеком!")
    exit()

processed_frames = 0
for jpeg_file in jpeg_files:
    frame = cv2.imread(jpeg_file)
    if frame is not None:
        if frame.shape[:2] != (height, width):
            frame = cv2.resize(frame, (width, height))
        video.write(frame)
        processed_frames += 1
        
        if processed_frames % 100 == 0:
            print(f"Оброблено {processed_frames}/{len(jpeg_files)} кадрів")
    else:
        print(f"Не вдалося прочитати файл: {jpeg_file}")

video.release()
print(f"Відео збережено: {output_video}")
print(f"Оброблено {processed_frames} кадрів з {len(jpeg_files)}")

if os.path.exists(output_video):
    file_size = os.path.getsize(output_video) / (1024 * 1024)  # MB
    print(f"Розмір файлу: {file_size:.1f} MB")