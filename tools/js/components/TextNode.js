import { UINode } from '../core/UINode.js';

/**
 * TextNode - Declarative 2D text element compatible with Nintendo 3DS Citro2D runtime.
 */
export class TextNode extends UINode {
  static schema = {
    displayName: 'Text',
    category: 'Typography',
    icon: '📝',
    description: 'Crisp 2D text element for Nintendo 3DS Citro2D rendering',
    capabilities: ['render', 'transform', 'animate', 'effects'],
    properties: {
      text: { type: 'string', default: 'Text' },
      font: { type: 'string', default: 'standard' },
      fontSize: { type: 'number', default: 12, min: 6, max: 48 },
      align: { type: 'enum', enum: ['left', 'center', 'right'], default: 'left' },
      color: { type: 'color', default: '#ffffff' },
      lineHeight: { type: 'number', default: 14, min: 8, max: 64 }
    }
  };

  constructor(data = {}) {
    super(data);
    this.type = 'Text';
    this.text = data.text !== undefined ? String(data.text) : (data.properties?.text ?? 'Text');
    this.font = data.font || data.properties?.font || 'standard';
    this.fontSize = Math.max(6, Math.round(data.fontSize ?? data.properties?.fontSize ?? 12));
    this.align = ['left', 'center', 'right'].includes(data.align || data.properties?.align)
      ? (data.align || data.properties?.align)
      : 'left';
    this.color = data.color || data.properties?.color || '#ffffff';
    this.lineHeight = Math.max(8, Math.round(data.lineHeight ?? data.properties?.lineHeight ?? 14));

    this.properties.text = this.text;
    this.properties.font = this.font;
    this.properties.fontSize = this.fontSize;
    this.properties.align = this.align;
    this.properties.color = this.color;
    this.properties.lineHeight = this.lineHeight;
  }

  getBounds() {
    return {
      x: this.x,
      y: this.y,
      width: this.width,
      height: this.height
    };
  }

  getDefaultProperties() {
    return {
      text: 'Text',
      font: 'standard',
      fontSize: 12,
      align: 'left',
      color: '#ffffff',
      lineHeight: 14
    };
  }

  draw(ctx, options = {}) {
    const evaluated = options.evaluatedMap?.get(this.id);
    const text = evaluated?.properties?.text ?? this.properties.text ?? this.text;
    const color = evaluated?.properties?.color ?? this.properties.color ?? this.color;
    const fontSize = evaluated?.properties?.fontSize ?? this.properties.fontSize ?? this.fontSize;
    const align = evaluated?.properties?.align ?? this.properties.align ?? this.align;
    const font = evaluated?.properties?.font ?? this.properties.font ?? this.font;

    ctx.save();

    // Apply any active effects
    const effects = evaluated?.effects || this.effects.getAll();
    for (const eff of effects) {
      if (eff.enabled === false) continue;
      if (eff.type === 'Opacity' && eff.parameters?.opacity !== undefined) {
        ctx.globalAlpha *= eff.parameters.opacity;
      }
      if (eff.type === 'Fade' && eff.parameters) {
        const { startAlpha = 0, endAlpha = 1, progress = 1 } = eff.parameters;
        const currentAlpha = startAlpha + (endAlpha - startAlpha) * progress;
        ctx.globalAlpha *= currentAlpha;
      }
    }

    ctx.fillStyle = color;
    ctx.textAlign = align;
    ctx.textBaseline = 'top';
    ctx.font = `${fontSize}px sans-serif`;

    let renderX = 0;
    if (align === 'center') renderX = this.width / 2;
    else if (align === 'right') renderX = this.width;

    ctx.fillText(text, renderX, 0);

    ctx.restore();
  }

  toJSON() {
    return {
      ...super.toJSON(),
      text: this.text,
      font: this.font,
      fontSize: this.fontSize,
      align: this.align,
      color: this.color,
      lineHeight: this.lineHeight,
      properties: {
        ...this.properties,
        text: this.text,
        font: this.font,
        fontSize: this.fontSize,
        align: this.align,
        color: this.color,
        lineHeight: this.lineHeight
      }
    };
  }
}
