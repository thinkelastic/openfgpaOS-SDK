from PIL import Image

img = Image.open('Tracker_mod_player_portada_320x240.png').convert('P', palette=Image.ADAPTIVE, colors=256)
data = list(img.getdata())
palette = img.getpalette()

# Generar el array de C
with open('image_data.h', 'w') as f:
    f.write('static const uint8_t image_map[76800] = {\n')
    f.write(','.join(map(str, data)))
    f.write('\n};')
    
    f.write('\n\nstatic const uint32_t palette[256] = {\n')
    for i in range(0, len(palette), 3):
        r, g, b = palette[i], palette[i+1], palette[i+2]
        f.write(f'0x{r:02x}{g:02x}{b:02x}, ')
    f.write('\n};')
