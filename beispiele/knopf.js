// Ein Zaehler-Knopf: JavaScript ohne Semikolons, Callbacks, ein Objekt mit Methoden.
const zaehler = {
  stand: 0,
  erhoehe(um = 1) {
    this.stand += um
    return this.stand
  },
  zuruecksetzen() {
    this.stand = 0
  },
}

function verbinde(knopf, anzeige) {
  knopf.addEventListener("click", (ereignis) => {
    if (ereignis.shiftKey) {
      zaehler.zuruecksetzen()
    } else {
      zaehler.erhoehe()
    }
    anzeige.textContent = `Stand: ${zaehler.stand}`
  })
  const muster = /^\d+$/g
  return muster.test(anzeige.textContent)
    ? "zahl"
    : "text"
}

document.querySelectorAll(".knopf").forEach(function (knopf) {
  verbinde(knopf, document.getElementById("anzeige"))
})
