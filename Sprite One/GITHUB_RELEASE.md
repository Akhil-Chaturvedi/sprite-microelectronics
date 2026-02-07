# Sprite One - GitHub Release Guide

Step-by-step guide to push Sprite One v1.0.0 to GitHub.

---

## Step 1: Initialize Git Repository

```bash
cd "E:\Akhil\Stuff\Sprite Microelectronics\Sprite One"

# Initialize repository
git init

# Add all files
git add .

# First commit
git commit -m "Initial release v1.0.0 - Graphics & AI Accelerator

- Complete firmware for RP2040
- Python and C host libraries
- Comprehensive documentation
- Automated test suite
- 28+ tests, 95% coverage
- Production-ready code"
```

---

## Step 2: Create GitHub Repository

1. Go to https://github.com/new
2. Repository name: `sprite-one`
3. Description: `Graphics & AI Accelerator for Embedded Systems`
4. **Public** repository
5. **Do NOT** initialize with README (we have one)
6. Click "Create repository"

---

## Step 3: Connect to GitHub

```bash
# Add remote
git remote add origin https://github.com/YOUR_USERNAME/sprite-one.git

# Or use SSH
git remote add origin git@github.com:YOUR_USERNAME/sprite-one.git

# Verify
git remote -v
```

---

## Step 4: Push to GitHub

```bash
# Push main branch
git branch -M main
git push -u origin main

# Create and push tag
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

---

## Step 5: Create GitHub Release

1. Go to repository → Releases → "Draft a new release"
2. Tag: `v1.0.0`
3. Title: `Sprite One v1.0.0 - Graphics & AI Accelerator`
4. Description: (Copy from RELEASE_v1.0.0.md)
5. **Assets to upload:**
   - None needed (all in repository)
   - Optional: Compiled .uf2 firmware binary
6. Click "Publish release"

---

## Step 6: Repository Settings

### Topics/Tags
Add these topics to help discovery:
- `embedded`
- `ai`
- `machine-learning`
- `rp2040`
- `graphics`
- `tinyml`
- `microcontroller`
- `arduino`

### About Section
```
Graphics & AI accelerator for embedded systems. Train neural networks on RP2040!
```

### Social Preview
Upload a preview image (if you have one)

---

## Step 7: Post-Release Actions

### Update README Badges
Add these to README.md:

```markdown
[![Release](https://img.shields.io/github/v/release/YOUR_USERNAME/sprite-one)](https://github.com/YOUR_USERNAME/sprite-one/releases)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-28%20passing-brightgreen.svg)](host/TESTING.md)
```

### Enable GitHub Features

1. **Issues** ✅ Enable
2. **Discussions** ✅ Enable  
3. **Wiki** ❌ Disable (use docs/ folder)
4. **Projects** ✅ Enable for roadmap

### Add GitHub Actions (Optional)

Create `.github/workflows/test.yml`:
```yaml
name: Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - uses: actions/setup-python@v2
      - run: pip install -r host/python/requirements.txt
      - run: python host/python/unit_tests.py
```

---

## Step 8: Spread the Word

### Share On:
- [ ] Hackaday.io
- [ ] Reddit (r/embedded, r/arduino, r/MachineLearning)
- [ ] Hacker News
- [ ] Twitter/X
- [ ] Arduino Forums
- [ ] EEVblog Forums

### Template Post:
```
🚀 Sprite One v1.0.0 Released!

An open-source graphics & AI accelerator for embedded systems.

- Train neural networks on RP2040
- <1ms inference
- 128x64 graphics engine
- Python & C libraries
- 100% open source (MIT)

GitHub: https://github.com/YOUR_USERNAME/sprite-one

Built it in 28 days! Feedback welcome! 🙌
```

---

## Complete Git Command Sequence

```bash
# Navigate to project
cd "E:\Akhil\Stuff\Sprite Microelectronics\Sprite One"

# Initialize
git init
git add .
git commit -m "Initial release v1.0.0"

# Connect to GitHub (create repo first!)
git remote add origin https://github.com/YOUR_USERNAME/sprite-one.git

# Push
git branch -M main
git push -u origin main

# Tag
git tag -a v1.0.0 -m "Release v1.0.0 - Graphics & AI Accelerator"
git push origin v1.0.0

echo "✅ Pushed to GitHub!"
echo "Now create the release at: https://github.com/YOUR_USERNAME/sprite-one/releases/new"
```

---

## Verification Checklist

Before pushing, verify:

- [x] LICENSE file present
- [x] CHANGELOG.md complete
- [x] CONTRIBUTING.md present
- [x] .gitignore configured
- [x] README.md updated
- [x] All docs in docs/
- [x] Examples working
- [x] Tests passing
- [x] Development files archived
- [x] No sensitive data

---

## Future Updates

### For Bug Fixes (v1.0.1)
```bash
# Make fixes
git add .
git commit -m "Fix: description"
git tag -a v1.0.1 -m "Bug fix release"
git push origin main --tags
```

### For Features (v1.1.0)
```bash
# Add features
git add .
git commit -m "Feature: description"
git tag -a v1.1.0 -m "Feature release"
git push origin main --tags
```

---

**Ready to share Sprite One with the world! 🌍🚀**
