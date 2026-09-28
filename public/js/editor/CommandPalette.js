/**
 * CommandPalette - Canonical action registry for production IDE operations.
 * Single source of truth for command execution avoiding duplicated logic.
 */
export class CommandPalette {
  constructor(handlers = {}) {
    this.handlers = { ...handlers };
    this._commands = new Map();
    this._initDefaultCommands();
  }

  registerDefaults(handlers = {}) {
    this.handlers = { ...this.handlers, ...handlers };
    this._initDefaultCommands();
  }

  _initDefaultCommands() {
    this.registerCommand({
      id: 'scene.new',
      aliases: ['new_scene', 'newscene'],
      title: 'New Scene',
      label: 'New Scene',
      category: 'Scene',
      shortcut: 'Ctrl+N',
      action: (ctx) => {
        if (this.handlers.onNewScene) this.handlers.onNewScene(ctx);
        return true;
      }
    });

    this.registerCommand({
      id: 'project.save',
      aliases: ['save_project', 'saveproject'],
      title: 'Save Project',
      label: 'Save Project',
      category: 'Project',
      shortcut: 'Ctrl+S',
      action: (ctx) => {
        if (this.handlers.onSaveProject) this.handlers.onSaveProject(ctx);
        return true;
      }
    });

    this.registerCommand({
      id: 'pipeline.export',
      aliases: ['export', 'build'],
      title: 'Export',
      label: 'Export',
      category: 'Build',
      shortcut: 'Ctrl+E',
      action: (ctx) => {
        if (this.handlers.onExport) this.handlers.onExport(ctx);
        return true;
      }
    });

    this.registerCommand({
      id: 'playback.play',
      aliases: ['play'],
      title: 'Play',
      label: 'Play',
      category: 'Timeline',
      shortcut: 'Space',
      action: (ctx) => {
        if (this.handlers.onPlay) this.handlers.onPlay(ctx);
        return true;
      }
    });

    this.registerCommand({
      id: 'playback.pause',
      aliases: ['pause'],
      title: 'Pause',
      label: 'Pause',
      category: 'Timeline',
      shortcut: 'Space',
      action: (ctx) => {
        if (this.handlers.onPause) this.handlers.onPause(ctx);
        return true;
      }
    });

    this.registerCommand({
      id: 'viewport.fit_screen',
      aliases: ['fit_screen', 'fitscreen'],
      title: 'Fit Screen',
      label: 'Fit Screen',
      category: 'Viewport',
      shortcut: 'F',
      action: (ctx) => {
        if (this.handlers.onFitScreen) this.handlers.onFitScreen(ctx);
        return true;
      }
    });

    this.registerCommand({
      id: 'timeline.add_keyframe',
      aliases: ['add_keyframe', 'addkeyframe'],
      title: 'Add Keyframe',
      label: 'Add Keyframe',
      category: 'Timeline',
      shortcut: 'K',
      action: (ctx) => {
        if (this.handlers.onAddKeyframe) this.handlers.onAddKeyframe(ctx);
        return true;
      }
    });

    this.registerCommand({
      id: 'composition.create',
      aliases: ['create_composition', 'createcomposition'],
      title: 'Create Composition',
      label: 'Create Composition',
      category: 'Composition',
      shortcut: 'Ctrl+Shift+C',
      action: (ctx) => {
        if (this.handlers.onCreateComposition) this.handlers.onCreateComposition(ctx);
        return true;
      }
    });
  }

  registerCommand(cmd) {
    if (!cmd || !cmd.id) return;
    const item = {
      id: cmd.id,
      aliases: cmd.aliases || [],
      title: cmd.title || cmd.id,
      label: cmd.label || cmd.title || cmd.id,
      category: cmd.category || 'General',
      shortcut: cmd.shortcut || '',
      action: cmd.action || (() => true)
    };
    this._commands.set(cmd.id, item);
  }

  getCommand(id) {
    if (!id) return null;
    if (this._commands.has(id)) return this._commands.get(id);
    const lower = String(id).toLowerCase().replace(/[\s.-]/g, '_');
    for (const cmd of this._commands.values()) {
      if (cmd.aliases.includes(lower) || cmd.id === lower) return cmd;
    }
    return null;
  }

  getCommands() {
    return Array.from(this._commands.values()).sort((a, b) => a.title.localeCompare(b.title));
  }

  filter(query) {
    if (!query) return this.getCommands();
    const q = query.toLowerCase().trim();
    return this.getCommands().filter(c =>
      c.title.toLowerCase().includes(q) ||
      c.label.toLowerCase().includes(q) ||
      c.category.toLowerCase().includes(q) ||
      c.id.toLowerCase().includes(q) ||
      c.aliases.some(a => a.includes(q))
    );
  }

  search(query) {
    return this.filter(query);
  }

  execute(commandId, context = {}) {
    const cmd = this.getCommand(commandId);
    if (!cmd) {
      return false;
    }
    const res = cmd.action(context);
    return res !== undefined ? res : true;
  }
}
