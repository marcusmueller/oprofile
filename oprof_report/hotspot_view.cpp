/* COPYRIGHT (C) 2002 Philippe Elie
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>

#include <string>

#include <qvariant.h>
#include <qpainter.h>

#include "../pp/opf_filter.h"

#include "hotspot_view.h"

/**
 * HotspotView - ctor
 *
 * create an HotspotView
 */
HotspotView::HotspotView(QWidget * parent)
	:
	QWidget(parent)
{ 
	setBackgroundMode(QWidget::PaletteBase);
	setBackgroundColor(QColor(80, 255, 80));
}

/**
 * paintEvent - handle painting event
 *
 * for now a simple and inneficient algorithm is used
 * all things are redrawn and we don't try to cache them
 */
/* TODO: axis painting, check why two paintEvent occur sometimes */
void HotspotView::paintEvent(QPaintEvent *)
{
	if (symbols.size() == 0)
		return;

	size_t last_sample_idx = symbols.back()->last;
	if (last_sample_idx == 0)
		return;

	// for now I show only the vma range which get samples, not the
	// application vma range, this is discussable.
	bfd_vma start_vma = samples->get_samples(0).vma;
	bfd_vma end_vma = samples->get_samples(last_sample_idx - 1).vma;
	unsigned long vma_range = end_vma - start_vma;

	// Horizontal axis : how many vma address per pixel.
	double nr_vma_per_pel = double(vma_range) / width();

	// vertical axis: how many sample nr per pixel.

	// this algorithm is very costly, use it for now because it is simple
	// more efficient will to iterate on samples rather to iterate on vma.
	u32 max_val = 0;
	std::vector<u32> samples_nr(width());
	for (size_t i = 0 ; i < samples_nr.size() ; ++i) {
		bfd_vma start = bfd_vma((i * nr_vma_per_pel) + start_vma);
		bfd_vma end = bfd_vma(start + nr_vma_per_pel);
		if (end > end_vma)
		    end = end_vma;

		if (end == start)
			end = start + 1;	// humm

		if (end != start) {
			for ( ; start != end ; ++start) {
				// get all sample belonging to this vma
				const sample_entry * sample =
					samples->find_sample(start);

				if (sample)
					samples_nr[i] += sample->counter[0];
			}
		}

		if (samples_nr[i] > max_val)
			max_val = samples_nr[i];
	}

	double nr_sample_per_pel = double(height()) / max_val;

	QPainter paint;
	paint.begin(this);

	for (size_t i = 0 ; i < samples_nr.size() ; ++i) {
		paint.drawLine(i, height(), 
			       i, int(height() - 
					(samples_nr[i] * nr_sample_per_pel)));
	}
	paint.end();
}

/**
 * do_data_change() - handle data change
 */
void HotspotView::do_data_change(const samples_files_t * samples_)
{
	samples = samples_;
	samples->select_symbols(symbols, 0, 0.0, false, true);

	update();
}

/**
 * do_data_destroy() - handle data destroy
 */
void HotspotView::do_data_destroy()
{
	symbols.clear();
}

