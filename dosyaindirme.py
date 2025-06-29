import os
import math
import requests

# Merkez koordinatlar (örnek: Bakırköy)
lat = 40.9769
lon = 28.8692
zoom = 16

# HTTP isteklerinde kullanılacak başlıklar
headers = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)"
}

# Lat/Lon → Tile koordinat dönüşümü (Slippy Map)
def deg2num(lat_deg, lon_deg, zoom):
    lat_rad = math.radians(lat_deg)
    n = 2.0 ** zoom
    xtile = int((lon_deg + 180.0) / 360.0 * n)
    ytile = int((1.0 - math.log(math.tan(lat_rad) + 1 / math.cos(lat_rad)) / math.pi) / 2.0 * n)
    return xtile, ytile

# Merkez tile koordinatlarını hesapla
center_x, center_y = deg2num(lat, lon, zoom)

# 6x6’lık alan için 3 tile sağ/sol ve yukarı/aşağı
tile_range = 3

# Belirlenen alan için tüm tile'ları indir
for x in range(center_x - tile_range, center_x + tile_range + 1):
    for y in range(center_y - tile_range, center_y + tile_range + 1):
        #url = f"https://a.basemaps.cartocdn.com/light_nolabels/{zoom}/{x}/{y}@2x.png" # yazısız
        url = f"https://tile.openstreetmap.org/{zoom}/{x}/{y}.png" # yazılı
        folder = f"tilesText/{zoom}/{x}"
        os.makedirs(folder, exist_ok=True)

        filepath = f"{folder}/{y}.png"
        if os.path.exists(filepath):
            print(f"✅ Zaten var: {filepath}")
            continue

        print(f"⬇️  İndiriliyor: {url}")
        response = requests.get(url, headers=headers)

        # Başarılıysa dosyayı kaydet
        if response.status_code == 200 and "image" in response.headers.get("Content-Type", ""):
            with open(filepath, "wb") as f:
                f.write(response.content)
        else:
            print(f"❌ Hata: {response.status_code} - {url}")
