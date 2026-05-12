#!/bin/bash
# setup.sh  —  Automatically patches xv6-riscv with CSE323 project features
# Run from inside ~/Downloads/xv6-riscv-riscv/
# Usage: bash setup.sh

set -e
echo "=== CSE323 xv6-riscv Setup Script ==="
echo "Kernel Explorers: Foysal Hasan Shifat & Marzia Hossain"
echo ""

# ── 1. Add priority & ctime fields to struct proc ────────────────────────────
echo "[1/6] Patching kernel/proc.h ..."
if ! grep -q "int priority" kernel/proc.h; then
  sed -i '' 's/  char name\[16\];/  char name[16];\n  int priority;        \/\/ 0=highest 10=lowest\n  uint64 ctime;        \/\/ creation time/' kernel/proc.h
  echo "      Done."
else
  echo "      Already patched."
fi

# ── 2. Patch allocproc() to set default priority ─────────────────────────────
echo "[2/6] Patching allocproc() in kernel/proc.c ..."
if ! grep -q "p->priority = 5" kernel/proc.c; then
  sed -i '' 's/p->pid = allocpid();/p->pid = allocpid();\n  p->priority = 5;\n  p->ctime = ticks;/' kernel/proc.c
  echo "      Done."
else
  echo "      Already patched."
fi

# ── 3. Patch fork() to inherit priority ──────────────────────────────────────
echo "[3/6] Patching fork() in kernel/proc.c ..."
if ! grep -q "np->priority = p->priority" kernel/proc.c; then
  sed -i '' 's/safestrcpy(np->name, p->name, sizeof(p->name));/safestrcpy(np->name, p->name, sizeof(p->name));\n  np->priority = p->priority;/' kernel/proc.c
  echo "      Done."
else
  echo "      Already patched."
fi

# ── 4. Add syscall numbers ────────────────────────────────────────────────────
echo "[4/6] Adding syscall numbers to kernel/syscall.h ..."
if ! grep -q "SYS_getchildren" kernel/syscall.h; then
  echo "#define SYS_getchildren 22" >> kernel/syscall.h
  echo "#define SYS_setpriority 23" >> kernel/syscall.h
  echo "      Done."
else
  echo "      Already patched."
fi

# ── 5. Add user-space prototypes ──────────────────────────────────────────────
echo "[5/6] Adding prototypes to user/user.h ..."
if ! grep -q "getchildren" user/user.h; then
  echo "int getchildren(int, int*, int);" >> user/user.h
  echo "int setpriority(int, int);" >> user/user.h
  echo "      Done."
else
  echo "      Already patched."
fi

# ── 6. Add user programs to Makefile ─────────────────────────────────────────
echo "[6/6] Adding test programs to Makefile ..."
if ! grep -q "test_getchildren" Makefile; then
  sed -i '' 's/\$U\/_zombie/\$U\/_zombie\\\n\t$U\/_test_getchildren\\\n\t$U\/_test_setpriority/' Makefile
  echo "      Done."
else
  echo "      Already patched."
fi

# ── Copy test files ───────────────────────────────────────────────────────────
echo ""
echo "Copying test programs..."
cp user/test_getchildren.c user/test_getchildren.c 2>/dev/null || true
cp user/test_setpriority.c user/test_setpriority.c 2>/dev/null || true

echo ""
echo "=== Setup complete! Now run: make qemu ==="
