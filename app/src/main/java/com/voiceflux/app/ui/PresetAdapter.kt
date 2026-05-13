package com.voiceflux.app.ui

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.voiceflux.app.R
import com.voiceflux.app.data.VoicePreset
import com.voiceflux.app.databinding.ItemPresetBinding

class PresetAdapter(
    private val onSelect: (VoicePreset) -> Unit
) : ListAdapter<VoicePreset, PresetAdapter.VH>(Diff) {

    private var selectedId: Int = 0

    fun setSelected(id: Int) {
        selectedId = id
        notifyDataSetChanged()
    }

    inner class VH(private val b: ItemPresetBinding) : RecyclerView.ViewHolder(b.root) {
        fun bind(preset: VoicePreset) {
            b.tvEmoji.text  = preset.emoji
            b.tvName.text   = preset.name
            val selected    = preset.id == selectedId
            b.root.isSelected = selected
            b.root.strokeWidth = if (selected) 3 else 0
            b.root.strokeColor = ContextCompat.getColorStateList(
                b.root.context, R.color.accent_purple
            )
            b.root.setOnClickListener { onSelect(preset) }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH =
        VH(ItemPresetBinding.inflate(LayoutInflater.from(parent.context), parent, false))

    override fun onBindViewHolder(holder: VH, position: Int) =
        holder.bind(getItem(position))

    object Diff : DiffUtil.ItemCallback<VoicePreset>() {
        override fun areItemsTheSame(a: VoicePreset, b: VoicePreset)   = a.id == b.id
        override fun areContentsTheSame(a: VoicePreset, b: VoicePreset) = a == b
    }
}
