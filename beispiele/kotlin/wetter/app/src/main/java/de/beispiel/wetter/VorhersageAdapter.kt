package de.beispiel.wetter

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import de.beispiel.wetter.daten.Einheit
import de.beispiel.wetter.daten.Vorhersage

class VorhersageAdapter(
    private val beiKlick: (Vorhersage.Tag) -> Unit,
) : RecyclerView.Adapter<VorhersageAdapter.Zeile>() {

    private val tage = mutableListOf<Vorhersage.Tag>()

    var einheit: Einheit = Einheit.CELSIUS
        set(neu) {
            if (field == neu) return
            field = neu
            notifyDataSetChanged()
        }

    fun zeige(neu: List<Vorhersage.Tag>) {
        val alt = tage.size
        tage.clear()
        tage.addAll(neu)
        when {
            alt == 0 -> notifyItemRangeInserted(0, neu.size)
            neu.isEmpty() -> notifyItemRangeRemoved(0, alt)
            else -> notifyDataSetChanged()
        }
    }

    override fun getItemCount() = tage.size

    override fun getItemViewType(position: Int): Int =
        if (position == 0) ART_HEUTE else ART_TAG

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Zeile {
        val layout = if (viewType == ART_HEUTE) R.layout.zeile_heute else R.layout.zeile_tag
        val ansicht = LayoutInflater.from(parent.context).inflate(layout, parent, false)
        return Zeile(ansicht)
    }

    override fun onBindViewHolder(holder: Zeile, position: Int) {
        holder.binde(tage[position])
    }

    inner class Zeile(ansicht: View) : RecyclerView.ViewHolder(ansicht) {
        private val datum: TextView = ansicht.findViewById(R.id.datum)
        private val werte: TextView = ansicht.findViewById(R.id.werte)

        init {
            ansicht.setOnClickListener {
                val stelle = bindingAdapterPosition
                if (stelle != RecyclerView.NO_POSITION) beiKlick(tage[stelle])
            }
        }

        fun binde(tag: Vorhersage.Tag) {
            datum.text = tag.datum
            werte.text = "${einheit.formatiere(tag.hoch)} / ${einheit.formatiere(tag.tief)}"
            werte.alpha = if (tag.unsicher) 0.6f else 1.0f
        }
    }

    companion object {
        const val ART_HEUTE = 0
        const val ART_TAG = 1
    }
}
