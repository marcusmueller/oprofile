/* COPYRIGHT (C) 2001 Philippe Elie
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
 *
 * first written by P.Elie
 */

#ifndef SHARED_PTR_H
#define SHARED_PTR_H

/**
 * A non intrusive shared pointer ala Copliend w/o invariant
 * for the Letter internal class and assuming than object is never nil.
 *
 * can be improved if neccessary by using a specialized allocator
 * for the letter pointer
 * unecessary operator for now are commented out.
 * don't add conversion operator to T* please, rather add 
 * explicit T& getter if necessary (or uncomment the T& conversion ?)
 */
template<class T> class SharedPtr
{
public:
	explicit SharedPtr(T * object) : letter(new Letter(object)) {}
	SharedPtr(const SharedPtr & src);
	~SharedPtr() { letter->release(); }

	SharedPtr & operator=(const SharedPtr & rhs);
	// a hole for bug ?
//	SharedPtr & operator=(T* object);

	T* operator->() { return letter->object; }
	const T* operator->() const { return letter->object; }

	// conversion to T* are dangerous can we allow conversion to T& ?
	T& operator *() { return *letter->object; }
	const T& operator *() const { return *letter->object; }

private:
	// Take care, this class have no invariant between public member call.
	// Invariant are maintained between public calls of SharedPtr. Safety
	// is provided only through the private decl of Letter.
	struct Letter
	{
		explicit Letter(T* object_) : object(object_), ref_count(1) {}
		~Letter() { delete object; }

		void addRef()  { ref_count++; }
		void release() { if (--ref_count == 0) delete this; }

		T*  object;
		int ref_count;
	};

	Letter * letter;
};

template <class T>
inline SharedPtr<T>::SharedPtr(const SharedPtr & src)
	:
	letter(src.letter)
{
	letter->addRef();
}

template<class T>
SharedPtr<T>& SharedPtr<T>::operator=(const SharedPtr<T>& rhs)
{
	rhs.letter->addRef();
	letter->release();
	letter = rhs.letter;

	return *this;
}

#if 0
// original implementation have this operator but that now look like bad
// at my eyes and I don't remember if a it's needed.
template<class T>
SharedPtr<T>& SharedPtr<T>::operator=(T* object)
{
	if (letter->object != object) {
		letter->release();
		letter = new Letter(object);  // assume object non null.
	}

	return *this;
}
#endif

#endif /* !SHARED_PTR_H */
