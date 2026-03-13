import sys


def write_result(path, status, message=""):
    with open(path, "w", encoding="utf-8") as result_file:
        result_file.write(f"{status}|{message}")


def main():
    if len(sys.argv) != 3:
        return 1

    image_path = sys.argv[1]
    result_path = sys.argv[2]

    try:
        from PIL import Image, UnidentifiedImageError
    except Exception:
        write_result(result_path, "PIL_MISSING", "Server image validation is unavailable. Install Pillow to validate uploads.")
        return 1

    try:
        with Image.open(image_path) as image:
            image.verify()
        write_result(result_path, "VALID")
        return 0
    except (UnidentifiedImageError, OSError):
        write_result(result_path, "INVALID", "Uploaded file is not a valid PNG, JPG, or JPEG image.")
        return 1
    except Exception:
        write_result(result_path, "ERROR", "Image validation failed on the server.")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())