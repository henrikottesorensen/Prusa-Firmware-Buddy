// Presence definitions for what a printer-with-an-enclosure build's Connect client references and a
// host build has not got: the enclosure hardware, the chamber filtration feature, and the filament
// catalogue the enclosure's INFO block enumerates.
//
// All three are reached from code compiled under XL_ENCLOSURE_SUPPORT() - render.cpp's `enclosure`
// object and planner.cpp's four EnclosureXxx SET_VALUE cases - and none of it is called by anything
// here: the render block is guarded on `enclosure_info.present`, which no Printer in this directory
// sets, and a server has no reason to send SET_VALUE for a part the printer never announced.
//
// Following missing_functions.cpp and the other mocks beside it - presence, not behaviour. Whoever
// gives one of these Printers a real enclosure needs real ones: the filament list here is empty, so
// `filtration_filaments` would render as [] rather than as the filaments that need filtering.
//
// <atomic> before the header because xl_enclosure.hpp declares a std::atomic member without
// including it; on target it arrives through another header first.

#include <atomic>

#include <xl_enclosure.hpp>
#include <feature/chamber_filtration/chamber_filtration.hpp>
#include <filament_list.hpp>

Enclosure xl_enclosure;

Enclosure::Enclosure() = default;

void Enclosure::setEnabled(bool) {}

namespace buddy {

uint32_t ChamberFiltration::filter_lifetime_s() const {
    return 0;
}

ChamberFiltration &chamber_filtration() {
    static ChamberFiltration instance;

    return instance;
}

} // namespace buddy

constinit const FilamentList all_filament_types {};

FilamentTypeParameters FilamentType::parameters() const {
    return {};
}
