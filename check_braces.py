lines = open(r'C:\Users\dossa\OneDrive\Desktop\Midnight-1\midnight-plugins\plugins\melody_maker\plugin_vst3.cpp', encoding='utf-8', errors='replace').readlines()
d = 0
cls = False
for i, l in enumerate(lines):
    if l.startswith('class MelodyMakerView'):
        cls = True
        d = 0
        print(f"Class starts at line {i+1}")
    if cls:
        d += l.count('{') - l.count('}')
        if d == 0 and i > 2942:
            print(f"Class closed at line {i+1}: {l.rstrip()[:80]}")
            break

if cls and d != 0:
    print(f"Class NEVER closed! Final depth={d}, last line={len(lines)}")
