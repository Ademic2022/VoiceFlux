package com.voiceflux.app.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Shader
import android.util.AttributeSet
import android.view.View
import kotlin.math.abs

/**
 * Real-time audio waveform visualiser drawn with a smooth gradient stroke.
 * Call [update] with fresh PCM samples whenever you have new data.
 */
class WaveformView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0
) : View(context, attrs, defStyle) {

    private var samples: FloatArray = FloatArray(512)

    // Automatic gain control: track a decaying peak so quiet signals fill the view
    private var agcPeak = 0.01f

    private val wavePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        strokeWidth = 3f
        style       = Paint.Style.STROKE
        strokeCap   = Paint.Cap.ROUND
        strokeJoin  = Paint.Join.ROUND
    }

    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }

    private val path     = Path()
    private val fillPath = Path()

    var activeColor: Int = Color.parseColor("#6C63FF")
        set(value) { field = value; invalidate() }

    var inactiveColor: Int = Color.parseColor("#2A2A4A")
        set(value) { field = value; invalidate() }

    var isActive: Boolean = false
        set(value) { field = value; invalidate() }

    fun update(data: FloatArray) {
        if (data.isEmpty()) return

        // Auto-gain: find peak, decay slowly, then normalise all samples so quiet
        // voice still fills the visualiser even at low mic levels.
        var peak = 0f
        for (s in data) { val a = abs(s); if (a > peak) peak = a }
        agcPeak = maxOf(agcPeak * 0.90f, peak, 0.005f)
        val gain = 1f / agcPeak

        samples = FloatArray(data.size) { (data[it] * gain).coerceIn(-1f, 1f) }
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val w = width.toFloat()
        val h = height.toFloat()
        val midY = h / 2f

        if (samples.isEmpty()) return

        val color = if (isActive) activeColor else inactiveColor

        // Gradient: full color at top/bottom, transparent at centre
        val gradient = LinearGradient(
            0f, 0f, 0f, h,
            intArrayOf(color, Color.TRANSPARENT, color),
            floatArrayOf(0f, 0.5f, 1f),
            Shader.TileMode.CLAMP
        )
        fillPaint.shader = gradient

        val step = w / samples.size.toFloat()
        path.reset()
        fillPath.reset()

        var x = 0f
        path.moveTo(x, midY - samples[0] * midY * 0.9f)
        fillPath.moveTo(x, midY)

        for (i in samples.indices) {
            x = i * step
            val amp = samples[i].coerceIn(-1f, 1f)
            val y   = midY - amp * midY * 0.9f
            path.lineTo(x, y)
            fillPath.lineTo(x, y)
        }
        fillPath.lineTo(x, midY)
        fillPath.close()

        wavePaint.color  = color
        canvas.drawPath(fillPath, fillPaint)
        canvas.drawPath(path,     wavePaint)
    }
}
