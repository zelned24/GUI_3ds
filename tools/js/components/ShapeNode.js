import { UINode } from '../core/UINode.js';

export const SHAPE_TYPES = ['Rectangle', 'RoundedRectangle', 'Line'];

export const ShapeTypes = {
  Rectangle: 'Rectangle',
  RoundedRectangle: 'RoundedRectangle',
  Line: 'Line'
};

/**
 * ShapeNode - 2D geometric vector shape element compatible with Nintendo 3DS Citro2D runtime.
 */
export class ShapeNode extends UINode {
  static schema = {
    displayName: 'Shape',
    category: 'Vector',
    icon: '🔷',
    description: 'Vector shape primitive (Rectangle, Rounded Rectangle, Line) for Citro2D',
    capabilities: ['render', 'transform', 'animate', 'effects'],
    properties: {
      shapeType: { type: 'enum', enum: SHAPE_TYPES, default: 'Rectangle' },
      fillColor: { type: 'color', default: '#3498db' },
      strokeColor: { type: 'color', default: '#2980b9' },
      strokeWidth: { type: 'number', default: 0, min: 0, max: 20 },
      cornerRadius: { type: 'number', default: 0, min: 0, max: 50 }
    }
  };

  constructor(data = {}) {
    super(data);
    this.type = 'Shape';
    this.shapeType = SHAPE_TYPES.includes(data.shapeType || data.properties?.shapeType)
      ? (data.shapeType || data.properties?.shapeType)
      : 'Rectangle';
    this.fillColor = data.fillColor || data.properties?.fillColor || '#3498db';
    this.strokeColor = data.strokeColor || data.properties?.strokeColor || '#2980b9';
    this.strokeWidth = Math.max(0, Math.round(data.strokeWidth ?? data.properties?.strokeWidth ?? 0));
    this.cornerRadius = Math.max(0, Math.round(data.cornerRadius ?? data.properties?.cornerRadius ?? 0));

    this.properties.shapeType = this.shapeType;
    this.properties.fillColor = this.fillColor;
    this.properties.strokeColor = this.strokeColor;
    this.properties.strokeWidth = this.strokeWidth;
    this.properties.cornerRadius = this.cornerRadius;
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
      shapeType: 'Rectangle',
      fillColor: '#3498db',
      strokeColor: '#2980b9',
      strokeWidth: 0,
      cornerRadius: 0
    };
  }

  draw(ctx, options = {}) {
    const evaluated = options.evaluatedMap?.get(this.id);
    const shapeType = evaluated?.properties?.shapeType ?? this.properties.shapeType ?? this.shapeType;
    const fillColor = evaluated?.properties?.fillColor ?? this.properties.fillColor ?? this.fillColor;
    const strokeColor = evaluated?.properties?.strokeColor ?? this.properties.strokeColor ?? this.strokeColor;
    const strokeWidth = evaluated?.properties?.strokeWidth ?? this.properties.strokeWidth ?? this.strokeWidth;
    const cornerRadius = evaluated?.properties?.cornerRadius ?? this.properties.cornerRadius ?? this.cornerRadius;

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

    const w = this.width;
    const h = this.height;

    if (shapeType === 'Line') {
      ctx.beginPath();
      ctx.moveTo(0, 0);
      ctx.lineTo(w, h);
      ctx.strokeStyle = strokeColor || fillColor;
      ctx.lineWidth = Math.max(1, strokeWidth || 1);
      ctx.stroke();
    } else if (shapeType === 'RoundedRectangle' && cornerRadius > 0) {
      const r = Math.min(cornerRadius, w / 2, h / 2);
      ctx.beginPath();
      ctx.moveTo(r, 0);
      ctx.lineTo(w - r, 0);
      if (typeof ctx.quadraticCurveTo === 'function') {
        ctx.quadraticCurveTo(w, 0, w, r);
        ctx.lineTo(w, h - r);
        ctx.quadraticCurveTo(w, h, w - r, h);
        ctx.lineTo(r, h);
        ctx.quadraticCurveTo(0, h, 0, h - r);
        ctx.lineTo(0, r);
        ctx.quadraticCurveTo(0, 0, r, 0);
      } else {
        ctx.rect(0, 0, w, h);
      }
      ctx.closePath();
      if (fillColor && fillColor !== 'transparent') {
        ctx.fillStyle = fillColor;
        ctx.fill();
      }
      if (strokeWidth > 0 && strokeColor && strokeColor !== 'transparent') {
        ctx.strokeStyle = strokeColor;
        ctx.lineWidth = strokeWidth;
        ctx.stroke();
      }
    } else {
      // Standard Rectangle
      if (fillColor && fillColor !== 'transparent') {
        ctx.fillStyle = fillColor;
        ctx.fillRect(0, 0, w, h);
      }
      if (strokeWidth > 0 && strokeColor && strokeColor !== 'transparent') {
        ctx.strokeStyle = strokeColor;
        ctx.lineWidth = strokeWidth;
        ctx.strokeRect(0, 0, w, h);
      }
    }

    ctx.restore();
  }

  toJSON() {
    return {
      ...super.toJSON(),
      shapeType: this.shapeType,
      fillColor: this.fillColor,
      strokeColor: this.strokeColor,
      strokeWidth: this.strokeWidth,
      cornerRadius: this.cornerRadius,
      properties: {
        ...this.properties,
        shapeType: this.shapeType,
        fillColor: this.fillColor,
        strokeColor: this.strokeColor,
        strokeWidth: this.strokeWidth,
        cornerRadius: this.cornerRadius
      }
    };
  }
}
