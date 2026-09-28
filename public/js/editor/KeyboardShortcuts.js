/**
 * KeyboardShortcuts - Consolidated shortcut manager with strict input element protection.
 */
export class KeyboardShortcuts {
  constructor(handlers = {}) {
    this.handlers = { ...handlers };
    this._enabled = true;
  }

  setEnabled(enabled) {
    this._enabled = Boolean(enabled);
  }

  isInputElement(target) {
    if (!target) return false;
    const tagName = target.tagName ? target.tagName.toUpperCase() : '';
    if (['INPUT', 'TEXTAREA', 'SELECT'].includes(tagName)) return true;
    if (target.isContentEditable) return true;
    return false;
  }

  handleKeyDown(event, context = {}) {
    if (!this._enabled || !event) return false;

    // Guard: Prevent shortcut trigger when typing inside text fields
    if (this.isInputElement(event.target)) {
      return false;
    }

    const key = event.key || '';
    const code = event.code || '';
    const ctrlOrCmd = Boolean(event.ctrlKey || event.metaKey);
    const shift = Boolean(event.shiftKey);

    // 1. Play / Pause (Space)
    if (key === ' ' || key === 'Spacebar' || code === 'Space') {
      const fn = this.handlers.onPlayPause || this.handlers.onTogglePlay;
      if (typeof fn === 'function') {
        if (typeof event.preventDefault === 'function') event.preventDefault();
        fn(context);
        return true;
      }
    }

    // 2. Add Keyframe (K)
    if ((key === 'k' || key === 'K' || code === 'KeyK') && !ctrlOrCmd) {
      if (typeof this.handlers.onAddKeyframe === 'function') {
        if (typeof event.preventDefault === 'function') event.preventDefault();
        this.handlers.onAddKeyframe(context);
        return true;
      }
    }

    // 3. Fit Screen (F)
    if ((key === 'f' || key === 'F' || code === 'KeyF') && !ctrlOrCmd) {
      if (typeof this.handlers.onFitScreen === 'function') {
        if (typeof event.preventDefault === 'function') event.preventDefault();
        this.handlers.onFitScreen(context);
        return true;
      }
    }

    // 4. Delete (Delete or Backspace)
    if (key === 'Delete' || key === 'Backspace' || code === 'Delete' || code === 'Backspace') {
      if (typeof this.handlers.onDelete === 'function') {
        if (typeof event.preventDefault === 'function') event.preventDefault();
        this.handlers.onDelete(context);
        return true;
      }
    }

    // 5. Copy (Ctrl/Cmd + C)
    if (ctrlOrCmd && (key === 'c' || key === 'C' || code === 'KeyC') && !shift) {
      if (typeof this.handlers.onCopy === 'function') {
        if (typeof event.preventDefault === 'function') event.preventDefault();
        this.handlers.onCopy(context);
        return true;
      }
    }

    // 6. Paste (Ctrl/Cmd + V)
    if (ctrlOrCmd && (key === 'v' || key === 'V' || code === 'KeyV') && !shift) {
      if (typeof this.handlers.onPaste === 'function') {
        if (typeof event.preventDefault === 'function') event.preventDefault();
        this.handlers.onPaste(context);
        return true;
      }
    }

    // 7. Undo (Ctrl/Cmd + Z)
    if (ctrlOrCmd && (key === 'z' || key === 'Z' || code === 'KeyZ') && !shift) {
      if (typeof this.handlers.onUndo === 'function') {
        if (typeof event.preventDefault === 'function') event.preventDefault();
        this.handlers.onUndo(context);
        return true;
      }
    }

    // 8. Redo (Ctrl/Cmd + Y or Ctrl/Cmd + Shift + Z)
    if ((ctrlOrCmd && (key === 'y' || key === 'Y' || code === 'KeyY')) ||
        (ctrlOrCmd && shift && (key === 'z' || key === 'Z' || code === 'KeyZ'))) {
      if (typeof this.handlers.onRedo === 'function') {
        if (typeof event.preventDefault === 'function') event.preventDefault();
        this.handlers.onRedo(context);
        return true;
      }
    }

    return false;
  }

  bind(target = null) {
    const el = target || (typeof window !== 'undefined' ? window : null);
    if (!el || typeof el.addEventListener !== 'function') return () => {};

    const listener = (e) => this.handleKeyDown(e);
    el.addEventListener('keydown', listener);
    return () => el.removeEventListener('keydown', listener);
  }
}
