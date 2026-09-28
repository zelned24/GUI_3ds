/**
 * CompositionNavigator - Breadcrumb navigation for editing nested compositions.
 * Maintains an editorial context stack without mutating the underlying SceneModel.
 */
export class CompositionNavigator {
  constructor(rootSceneOrId = 'MainScene', rootSceneName = null) {
    let rootId = 'MainScene';
    let rootName = 'Main Scene';
    if (rootSceneOrId && typeof rootSceneOrId === 'object') {
      rootId = rootSceneOrId.id || 'MainScene';
      rootName = rootSceneOrId.name || rootId;
    } else if (typeof rootSceneOrId === 'string') {
      rootId = rootSceneOrId;
      rootName = rootSceneName || rootId;
    }
    this._stack = [{ sceneId: rootId, name: rootName }];
    this.listeners = [];
  }

  setRoot(sceneOrId, name = null) {
    let sId = sceneOrId;
    let sName = name;
    if (sceneOrId && typeof sceneOrId === 'object') {
      sId = sceneOrId.id;
      sName = sceneOrId.name || sId;
    }
    this._stack = [{ sceneId: sId, name: sName || sId }];
    this._emit();
  }

  enterComposition(compOrId, name = null) {
    if (!compOrId) return;
    let cId = compOrId;
    let cName = name;
    if (typeof compOrId === 'object') {
      cId = compOrId.id || compOrId.compositionId;
      cName = compOrId.name || compOrId.id || name || cId;
    }
    this._stack.push({
      sceneId: cId,
      name: cName || cId
    });
    this._emit();
  }

  exitComposition() {
    if (this._stack.length > 1) {
      const popped = this._stack.pop();
      this._emit();
      return popped;
    }
    return null;
  }

  exitToRoot() {
    while (this._stack.length > 1) {
      this._stack.pop();
    }
    this._emit();
  }

  navigateTo(index) {
    if (index >= 0 && index < this._stack.length) {
      this._stack = this._stack.slice(0, index + 1);
      this._emit();
    }
  }

  getCurrentSceneId() {
    return this._stack[this._stack.length - 1].sceneId;
  }

  getCurrentName() {
    return this._stack[this._stack.length - 1].name;
  }

  getBreadcrumbs() {
    return this._stack.map(item => ({ ...item }));
  }

  getBreadcrumbPath() {
    return this._stack.map(item => item.name).join(' / ');
  }

  getBreadcrumbsString() {
    return this.getBreadcrumbPath();
  }

  isAtRoot() {
    return this._stack.length === 1;
  }

  getDepth() {
    return this._stack.length - 1;
  }

  isInsideComposition() {
    return this._stack.length > 1;
  }

  getDepth() {
    return this._stack.length - 1;
  }

  subscribe(listener) {
    this.listeners.push(listener);
    return () => {
      this.listeners = this.listeners.filter(l => l !== listener);
    };
  }

  _emit() {
    const current = this.getCurrentSceneId();
    const path = this.getBreadcrumbPath();
    for (const l of this.listeners) {
      try { l(current, path); } catch (err) { console.error('CompositionNavigator error:', err); }
    }
  }
}
