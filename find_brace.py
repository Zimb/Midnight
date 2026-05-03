lines = open(r'midnight-plugins\plugins\melody_maker\plugin_vst3.cpp', encoding='utf-8', errors='replace').readlines()
d = 0
cls = False
results = []
for i, l in enumerate(lines):
    if i == 2940:
        cls = True
        d = 0
    if cls:
        nd = d + l.count('{') - l.count('}')
        if nd != d:
            results.append(f'L{i+1:5d} {d:+d}->{nd:+d}  {l.rstrip()[:70]}')
        d = nd

# Show last 40 lines
for r in results[-40:]:
    print(r)
print(f"\nFinal depth: {d}")
