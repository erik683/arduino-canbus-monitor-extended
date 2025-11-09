# Git & Open Source Modification Cheatsheet

## 🚀 Basic Git Commands

### Repository Status & Sync

```bash
git status                    # Check what's changed/modified
git log --oneline -10         # See recent commits (last 10)
git remote -v                 # Check remote repository URLs
git branch -a                 # List all branches (local + remote)
```

### Sync with Remote

```bash
git pull origin master        # Get latest changes from remote
git push origin master        # Send your changes to remote
git fetch origin              # Download changes without merging
```

### Working with Changes

```bash
git add <filename>            # Stage specific file for commit
git add .                     # Stage all changes
git commit -m "message"       # Save staged changes with message
git reset --hard origin/master # Reset to match remote exactly
git clean -fd                 # Remove untracked files/directories
```

## 🎯 Best Practices for Modifying Someone Else's Work

### Before You Start

1. **Fork the repository** on GitHub (if you want to share your changes)
2. **Clone your fork** (or the original if read-only)
3. **Create a feature branch** for your modifications
4. **Document your changes** as you go

### Development Workflow

```bash
# Start with a clean slate
git checkout master
git pull origin master

# Create feature branch for your changes
git checkout -b feature/my-modification

# Make your changes, then:
git add .
git commit -m "Add: descriptive message of what you changed"

# Push your feature branch
git push origin feature/my-modification
```

### When Porting/Bridging to Another Device

#### 1. Initial Setup

- **Document the target device** in comments/code
- **Keep original code structure** where possible for maintainability
- **Use clear naming** for device-specific modifications

#### 2. Code Organization

```text
src/
├── original_device/
│   ├── driver.c
│   └── config.h
└── my_device_port/
    ├── driver.c          # Ported version
    ├── config.h          # Device-specific config
    └── README.md         # Porting notes
```

#### 3. Version Control Strategy

```bash
# Use tags for different versions
git tag v1.0-original        # Original codebase
git tag v1.1-my-device-port  # Your ported version

# Document changes in commit messages
git commit -m "Port: Adapt CAN interface for DeviceX
- Changed SPI pins from 11,12,13 to 5,18,23
- Modified baud rate detection for DeviceX timing
- Added DeviceX-specific error handling"
```

### Adding Personal Functionality

#### 1. Feature Flags

```c
// Use preprocessor flags for optional features
#define ENABLE_DEBUG_LOGGING
#define SUPPORT_DEVICE_X
#define CUSTOM_BAUD_RATES

#ifdef ENABLE_DEBUG_LOGGING
    Serial.println("Debug: CAN message received");
#endif
```

#### 2. Configuration Files

- Create `config.h` for user-customizable settings
- Document all options with comments
- Provide sensible defaults

#### 3. Backward Compatibility

- Don't break existing functionality
- Test with original use cases
- Mark breaking changes clearly

### Pull Request Best Practices

#### If Contributing Back

1. **Test thoroughly** - ensure no regressions
2. **Write clear PR description**:

   ```markdown
   ## What Changed
   - Added support for DeviceX
   - Modified CAN timing for compatibility

   ## Testing
   - Verified on Arduino Uno
   - Tested with existing CAN devices
   - No breaking changes to original functionality

   ## Usage
   Define SUPPORT_DEVICEX in config.h
   ```

3. **Keep PRs focused** - one feature per PR
4. **Respond to feedback** promptly

#### For Personal Use Only

- **Document your changes** in a personal README
- **Keep track of versions** you modify
- **Consider sharing** useful improvements back to community

## 🔧 Common Scenarios & Solutions

### "I broke something - help!"

```bash
# See what changed
git status
git diff

# Undo uncommitted changes
git checkout -- <filename>
git reset --hard HEAD

# Go back to last commit
git reset --hard HEAD~1
```

### "I want to try something risky"

```bash
# Create experimental branch
git checkout -b experimental/feature
# If it works great, merge back
# If not, just delete the branch
git branch -D experimental/feature
```

### "Merge conflict - what now?"

```bash
# Don't panic! Git will show you the conflicts
git status                    # See conflicted files
# Edit files to resolve conflicts
# Then:
git add <resolved_file>
git commit -m "Resolve merge conflicts"
```

## 📚 Learning Resources

- [Git Documentation](https://git-scm.com/doc)
- [GitHub Flow Guide](https://guides.github.com/introduction/flow/)
- [Open Source Contribution Guidelines](https://opensource.guide/how-to-contribute/)

## 🎨 Personal Tips

1. **Commit often** with clear messages
2. **Branch early** for experimental features
3. **Document everything** - future you will thank present you
4. **Test on multiple devices** when porting
5. **Share improvements** if they're generally useful
6. **Keep original attribution** and licenses intact

---

*Created for arduino-canbus-monitor project - customize as needed!*
