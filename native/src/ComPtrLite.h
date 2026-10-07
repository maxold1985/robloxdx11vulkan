#pragma once

#include <utility>

template <typename T>
class ComPtr
{
public:
	ComPtr()
		: pointer_(nullptr)
	{
	}

	ComPtr(std::nullptr_t)
		: pointer_(nullptr)
	{
	}

	explicit ComPtr(T* pointer)
		: pointer_(pointer)
	{
		internalAddRef();
	}

	ComPtr(const ComPtr& other)
		: pointer_(other.pointer_)
	{
		internalAddRef();
	}

	ComPtr(ComPtr&& other) noexcept
		: pointer_(other.pointer_)
	{
		other.pointer_ = nullptr;
	}

	~ComPtr()
	{
		internalRelease();
	}

	ComPtr& operator=(
		const ComPtr& other
	)
	{
		if (this == &other)
			return *this;

		T* newPointer =
			other.pointer_;

		if (newPointer)
			newPointer->AddRef();

		internalRelease();

		pointer_ = newPointer;
		return *this;
	}

	ComPtr& operator=(
		ComPtr&& other
	) noexcept
	{
		if (this == &other)
			return *this;

		internalRelease();

		pointer_ = other.pointer_;
		other.pointer_ = nullptr;

		return *this;
	}

	ComPtr& operator=(std::nullptr_t)
	{
		Reset();
		return *this;
	}

	T* Get() const
	{
		return pointer_;
	}

	T** GetAddressOf()
	{
		return &pointer_;
	}

	T* const* GetAddressOf() const
	{
		return &pointer_;
	}

	T** ReleaseAndGetAddressOf()
	{
		internalRelease();
		return &pointer_;
	}

	void Reset()
	{
		internalRelease();
	}

	void Attach(T* pointer)
	{
		if (pointer_ == pointer)
			return;

		internalRelease();
		pointer_ = pointer;
	}

	T* Detach()
	{
		T* result = pointer_;
		pointer_ = nullptr;
		return result;
	}

	T* operator->() const
	{
		return pointer_;
	}

	explicit operator bool() const
	{
		return pointer_ != nullptr;
	}

	operator T*() const
	{
		return pointer_;
	}

	// Mantém compatibilidade com código Win32/DX11 que usa:
	// CreateXxx(..., &comPtr)
	//
	// Antes de receber um novo ponteiro COM, libera o anterior
	// para evitar vazamento.
	T** operator&()
	{
		return ReleaseAndGetAddressOf();
	}

private:
	void internalAddRef()
	{
		if (pointer_)
			pointer_->AddRef();
	}

	void internalRelease()
	{
		T* pointer = pointer_;

		if (!pointer)
			return;

		pointer_ = nullptr;
		pointer->Release();
	}

	T* pointer_;
};
