# -*- mode: python ; coding: utf-8 -*-
"""
PyInstaller spec for the 4-Stem Flask sidecar.
Run from the python/ directory:
    pyinstaller sidecar.spec
"""
from PyInstaller.utils.hooks import collect_all, collect_submodules

datas_all   = []
binaries_all = []
hidden_all  = []

# Collect all data / binaries / hidden imports from heavy ML packages
for pkg in ['torch', 'torchaudio', 'demucs', 'audio_separator']:
    try:
        d, b, h = collect_all(pkg)
        datas_all   += d
        binaries_all += b
        hidden_all  += h
    except Exception as e:
        print(f"[spec] Warning: could not collect {pkg}: {e}")

hidden_all += [
    # Flask stack
    'flask', 'werkzeug', 'werkzeug.serving', 'werkzeug.debug',
    'click', 'jinja2', 'itsdangerous', 'markupsafe',
    # ML
    'onnxruntime', 'onnxruntime.capi', 'onnxruntime.capi.onnxruntime_pybind11_state',
    # Demucs deps
    'julius', 'einops', 'lameenc', 'openunmix', 'diffq', 'dora', 'treetable',
    # Audio
    'numpy', 'scipy', 'scipy.signal', 'soundfile', 'librosa',
    'audioread', 'resampy', 'samplerate',
    # App modules
    'separate', 'server',
]

a = Analysis(
    ['sidecar.py'],
    pathex=['.'],
    binaries=binaries_all,
    datas=datas_all,
    hiddenimports=hidden_all,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[
        'matplotlib', 'IPython', 'jupyter', 'PIL',
        'cv2', 'sklearn', 'pandas', 'tkinter',
    ],
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=None)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='sidecar',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=True,   # keep console for stdout (Electron reads SIDECAR_PORT from it)
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='sidecar',
)
