// Copyright (c) Microsoft Corporation. All rights reserved.
//
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.

#include "pch.h"

#include "CanvasImageBrush.h"

using namespace ABI::Microsoft::Graphics::Canvas;
using namespace ABI::Microsoft::Graphics::Canvas::Brushes;
using namespace ABI::Windows::UI;
using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL::Wrappers;


IFACEMETHODIMP CanvasImageBrushFactory::Create(
    ICanvasResourceCreator* resourceAllocator,
    ICanvasImageBrush** canvasImageBrush)
{
    return CreateWithImage(
        resourceAllocator,
        nullptr,
        canvasImageBrush);
}

IFACEMETHODIMP CanvasImageBrushFactory::CreateWithImage(
    ICanvasResourceCreator* resourceAllocator,
    ICanvasImage* image,
    ICanvasImageBrush** canvasImageBrush)
{
    return ExceptionBoundary(
        [&]
        {
            CheckInPointer(resourceAllocator);
            CheckAndClearOutPointer(canvasImageBrush);

            ComPtr<ICanvasDevice> device;
            ThrowIfFailed(resourceAllocator->get_Device(&device));

            auto newImageBrush = Make<CanvasImageBrush>(
                device.Get(),
                image);
            CheckMakeResult(newImageBrush);

            ThrowIfFailed(newImageBrush.CopyTo(canvasImageBrush));
        });
}

CanvasImageBrush::CanvasImageBrush(
    ICanvasDevice* device,
    ID2D1BitmapBrush1* bitmapBrush)
    : CanvasBrush(device)
    , ResourceWrapper(bitmapBrush)
    , m_d2dBitmapBrush(bitmapBrush)
    , m_isSourceRectSet(false)
{
}

CanvasImageBrush::CanvasImageBrush(
    ICanvasDevice* device,
    ID2D1ImageBrush* imageBrush)
    : CanvasBrush(device)
    , ResourceWrapper(imageBrush)
    , m_d2dImageBrush(imageBrush)
    , m_isSourceRectSet(true)
{
}

CanvasImageBrush::CanvasImageBrush(
    ICanvasDevice* device,
    ICanvasImage* image)
    : CanvasBrush(device)
    , ResourceWrapper(nullptr)
    , m_isSourceRectSet(false)
{
    if (image)
    {
        SetImage(image);
    }
    else
    {
        SwitchToBitmapBrush(nullptr);
    }
}

void CanvasImageBrush::SetImage(ICanvasImage* image)
{
    m_effectNeedingDpiFixup.Reset();

    if (image == nullptr)
    {
        if (m_d2dBitmapBrush)
            m_d2dBitmapBrush->SetBitmap(nullptr);
        else
            m_d2dImageBrush->SetImage(nullptr);

        return;
    }

    auto bitmap = MaybeAs<ICanvasBitmap>(image);

    if (bitmap && !m_isSourceRectSet)
    {
        // Use a bitmap brush.
        auto& d2dBitmap = As<ICanvasBitmapInternal>(bitmap)->GetD2DBitmap();

        if (m_d2dBitmapBrush)
        {
            m_d2dBitmapBrush->SetBitmap(d2dBitmap.Get());
        }
        else
        {
            SwitchToBitmapBrush(d2dBitmap.Get());
        }
    }
    else
    {
        // Use an image brush.
        auto d2dImage = As<ICanvasDeviceInternal>(m_device.EnsureNotClosed())->GetD2DImage(image);

        if (m_d2dImageBrush)
        {
            m_d2dImageBrush->SetImage(d2dImage.Get());
        }
        else
        {
            SwitchToImageBrush(d2dImage.Get());
        }

        // Effects need to be reconfigured depending on the DPI of the device context they
        // are drawn onto. We don't know target DPI at this point, so if the image is an
        // effect, we store that away for use by a later fixup inside GetD2DBrush.
        auto effect = MaybeAs<IGraphicsEffect>(image);
            
        if (effect)
        {
            m_effectNeedingDpiFixup = As<ICanvasImageInternal>(effect);
        }
    }
}

IFACEMETHODIMP CanvasImageBrush::get_Image(ICanvasImage** value)
{        
    return ExceptionBoundary(
        [&]
        {
            CheckAndClearOutPointer(value);
            auto& device = m_device.EnsureNotClosed();

            auto d2dImage = GetD2DImage();

            if (!d2dImage)
                return;

            auto image = ResourceManager::GetOrCreate<ICanvasImage>(device.Get(), d2dImage.Get());
            ThrowIfFailed(image.CopyTo(value));
        });
}

ComPtr<ID2D1Image> CanvasImageBrush::GetD2DImage() const
{
    Lock lock(m_mutex);
    
    if (m_d2dBitmapBrush)
    {
        ComPtr<ID2D1Bitmap> bitmap;
        m_d2dBitmapBrush->GetBitmap(&bitmap);

        return bitmap;
    }
    else
    {
        ComPtr<ID2D1Image> image;
        m_d2dImageBrush->GetImage(&image);

        return image;
    }
}

IFACEMETHODIMP CanvasImageBrush::put_Image(ICanvasImage* value)
{
    return ExceptionBoundary(
        [&]
        {
            Lock lock(m_mutex);    

            ThrowIfClosed();

            SetImage(value);
        });
}

IFACEMETHODIMP CanvasImageBrush::get_ExtendX(CanvasEdgeBehavior* value)
{
    return ExceptionBoundary(
        [&]
        {
            Lock lock(m_mutex);    

            CheckInPointer(value);
            ThrowIfClosed();
            D2D1_EXTEND_MODE extendMode;

            if (m_d2dBitmapBrush)
                extendMode = m_d2dBitmapBrush->GetExtendModeX();
            else 
                extendMode = m_d2dImageBrush->GetExtendModeX();

            *value = static_cast<CanvasEdgeBehavior>(extendMode);
        });
}

IFACEMETHODIMP CanvasImageBrush::put_ExtendX(CanvasEdgeBehavior value)
{
    return ExceptionBoundary(
        [&]
        {
            Lock lock(m_mutex);    

            ThrowIfClosed();

            if (m_d2dBitmapBrush)
                m_d2dBitmapBrush->SetExtendModeX(static_cast<D2D1_EXTEND_MODE>(value));
            else 
                m_d2dImageBrush->SetExtendModeX(static_cast<D2D1_EXTEND_MODE>(value));
        });
}

IFACEMETHODIMP CanvasImageBrush::get_ExtendY(CanvasEdgeBehavior* value)
{
    return ExceptionBoundary(
        [&]
        {
            Lock lock(m_mutex);    

            CheckInPointer(value);
            ThrowIfClosed();
            D2D1_EXTEND_MODE extendMode;

            if (m_d2dBitmapBrush)
                extendMode = m_d2dBitmapBrush->GetExtendModeY();
            else 
                extendMode = m_d2dImageBrush->GetExtendModeY();

            *value = static_cast<CanvasEdgeBehavior>(extendMode);
        });
}

IFACEMETHODIMP CanvasImageBrush::put_ExtendY(CanvasEdgeBehavior value)
{
    return ExceptionBoundary(
        [&]
        {
            Lock lock(m_mutex);    

            ThrowIfClosed();

            if (m_d2dBitmapBrush)
                m_d2dBitmapBrush->SetExtendModeY(static_cast<D2D1_EXTEND_MODE>(value));
            else 
                m_d2dImageBrush->SetExtendModeY(static_cast<D2D1_EXTEND_MODE>(value));
        });
}

IFACEMETHODIMP CanvasImageBrush::get_SourceRectangle(ABI::Windows::Foundation::IReference<ABI::Windows::Foundation::Rect>** value)
{
    return ExceptionBoundary(
        [&]
        {
            Lock lock(m_mutex);    

            CheckAndClearOutPointer(value);
            ThrowIfClosed();

            ComPtr<IReference<Rect>> rectReference;

            if (m_d2dImageBrush && m_isSourceRectSet)
            {
                D2D1_RECT_F sourceRectangle;
                m_d2dImageBrush->GetSourceRectangle(&sourceRectangle);

                rectReference = Make<Nullable<Rect>>(FromD2DRect(sourceRectangle));
                CheckMakeResult(rectReference);
            }

            *value = rectReference.Detach(); // returns a NULL rect for m_useBitmapBrush.
        });
}

static D2D1_RECT_F GetD2DRectFromRectReference(ABI::Windows::Foundation::IReference<ABI::Windows::Foundation::Rect>* value)
{
    Rect rect{};
    ThrowIfFailed(value->get_Value(&rect));
    return ToD2DRect(rect);
}

IFACEMETHODIMP CanvasImageBrush::put_SourceRectangle(ABI::Windows::Foundation::IReference<ABI::Windows::Foundation::Rect>* value)
{
    return ExceptionBoundary(
        [&]
        {
            Lock lock(m_mutex);    

            ThrowIfClosed();

            if (m_d2dBitmapBrush)
            {
                assert(!m_isSourceRectSet);
                if (value)
                {
                    ComPtr<ID2D1Bitmap> targetImage;
                    m_d2dBitmapBrush->GetBitmap(&targetImage);

                    SwitchToImageBrush(targetImage.Get());

                    D2D1_RECT_F d2dRect = GetD2DRectFromRectReference(value);

                    m_d2dImageBrush->SetSourceRectangle(&d2dRect);
                    m_isSourceRectSet = true;
                }
                // If value is null, do nothing.
            }
            else
            {
                if (value)
                {
                    D2D1_RECT_F d2dRect = GetD2DRectFromRectReference(value);

                    m_d2dImageBrush->SetSourceRectangle(&d2dRect);
                    m_isSourceRectSet = true;
                }
                else
                {
                    D2D1_RECT_F defaultRect{};
                    m_d2dImageBrush->SetSourceRectangle(&defaultRect);
                    m_isSourceRectSet = false;

                    // Source rect is null. We might be able to switch to
                    // bitmap brush.
                    TrySwitchFromImageBrushToBitmapBrush();
                }
            }
        });
}

IFACEMETHODIMP CanvasImageBrush::get_Interpolation(CanvasImageInterpolation* value)
{
    return ExceptionBoundary(
        [&]
        {
            Lock lock(m_mutex);    

            CheckInPointer(value);
            ThrowIfClosed();
            D2D1_INTERPOLATION_MODE interpolationMode;

            if (m_d2dBitmapBrush)
                interpolationMode = m_d2dBitmapBrush->GetInterpolationMode1();
            else 
                interpolationMode = m_d2dImageBrush->GetInterpolationMode();

            *value = static_cast<CanvasImageInterpolation>(interpolationMode);
        });
}

IFACEMETHODIMP CanvasImageBrush::put_Interpolation(CanvasImageInterpolation value)
{
    return ExceptionBoundary(
        [&]()
        {
            Lock lock(m_mutex);    

            ThrowIfClosed();

            if (m_d2dBitmapBrush)
                m_d2dBitmapBrush->SetInterpolationMode1(static_cast<D2D1_INTERPOLATION_MODE>(value));
            else 
                m_d2dImageBrush->SetInterpolationMode(static_cast<D2D1_INTERPOLATION_MODE>(value));
        });
}

IFACEMETHODIMP CanvasImageBrush::Close()
{
    CanvasBrush::Close();
    m_d2dBitmapBrush.Reset();
    m_d2dImageBrush.Reset();
    return ResourceWrapper::Close();
}

ComPtr<ID2D1Brush> CanvasImageBrush::GetD2DBrush(ID2D1DeviceContext* deviceContext, GetBrushFlags flags)
{
    Lock lock(m_mutex);    

    ThrowIfClosed();

    if (m_d2dBitmapBrush)
    {
        return m_d2dBitmapBrush;
    }
    else 
    {
        // Validate the brush configuration?
        if ((flags & GetBrushFlags::NoValidation) == GetBrushFlags::None)
        {
            if (!m_isSourceRectSet)
            {
                ThrowHR(E_INVALIDARG, Strings::ImageBrushRequiresSourceRectangle);
            }
        }

        // If our input image is an effect graph, make sure it is fully configured to match the target DPI.
        if (m_effectNeedingDpiFixup && deviceContext)
        {
            float targetDpi = ((flags & GetBrushFlags::AlwaysInsertDpiCompensation) != GetBrushFlags::None) ? MAGIC_FORCE_DPI_COMPENSATION_VALUE : GetDpi(deviceContext);

            m_effectNeedingDpiFixup->GetRealizedEffectNode(deviceContext, targetDpi);
        }

        return m_d2dImageBrush;
    }
}

IFACEMETHODIMP CanvasImageBrush::GetResource(ICanvasDevice* device, float dpi, REFIID iid, void** resource)
{
    UNREFERENCED_PARAMETER(dpi);

    return ExceptionBoundary(
        [=]
        {
            CheckAndClearOutPointer(resource);

            Lock lock(m_mutex);    

            ThrowIfClosed();

            ResourceManager::ValidateDevice(static_cast<ICanvasResourceWrapperWithDevice*>(this), device);

            // TODO #1697: pass target DPI on to our source effect once we properly support effect interop.
            // if (m_effectNeedingDpiFixup)
            //    m_effectNeedingDpiFixup->GetResource(device, dpi);

            if (m_d2dBitmapBrush)
                ThrowIfFailed(m_d2dBitmapBrush.CopyTo(iid, resource));
            else
                ThrowIfFailed(m_d2dImageBrush.CopyTo(iid, resource));
        });
}

void CanvasImageBrush::ThrowIfClosed()
{
    m_device.EnsureNotClosed();
}

void CanvasImageBrush::SwitchToImageBrush(ID2D1Image* image)
{
    assert(!m_d2dImageBrush);

    m_d2dImageBrush = As<ICanvasDeviceInternal>(m_device.EnsureNotClosed())->CreateImageBrush(image);

    if (m_d2dBitmapBrush)
    {
        m_d2dImageBrush->SetExtendModeX(m_d2dBitmapBrush->GetExtendModeX());
        m_d2dImageBrush->SetExtendModeY(m_d2dBitmapBrush->GetExtendModeY());
        m_d2dImageBrush->SetInterpolationMode(m_d2dBitmapBrush->GetInterpolationMode1());
        m_d2dImageBrush->SetOpacity(m_d2dBitmapBrush->GetOpacity());
        D2D1_MATRIX_3X2_F transform;
        m_d2dBitmapBrush->GetTransform(&transform);
        m_d2dImageBrush->SetTransform(transform);

        m_d2dBitmapBrush.Reset();
    }

    SetResource(m_d2dImageBrush.Get());
}

void CanvasImageBrush::SwitchToBitmapBrush(ID2D1Bitmap1* bitmap)
{
    assert(!m_d2dBitmapBrush);
    assert(!m_isSourceRectSet);

    m_d2dBitmapBrush = As<ICanvasDeviceInternal>(m_device.EnsureNotClosed())->CreateBitmapBrush(bitmap);

    if (m_d2dImageBrush)
    {
        m_d2dBitmapBrush->SetExtendModeX(m_d2dImageBrush->GetExtendModeX());
        m_d2dBitmapBrush->SetExtendModeY(m_d2dImageBrush->GetExtendModeY());
        m_d2dBitmapBrush->SetInterpolationMode1(m_d2dImageBrush->GetInterpolationMode());
        m_d2dBitmapBrush->SetOpacity(m_d2dImageBrush->GetOpacity());
        D2D1_MATRIX_3X2_F transform;
        m_d2dImageBrush->GetTransform(&transform);
        m_d2dBitmapBrush->SetTransform(transform);

        m_d2dImageBrush.Reset();
    }

    SetResource(m_d2dBitmapBrush.Get());
}

void CanvasImageBrush::TrySwitchFromImageBrushToBitmapBrush()
{
    assert(m_d2dImageBrush);
    assert(!m_isSourceRectSet);

    ComPtr<ID2D1Image> targetImage;
    m_d2dImageBrush->GetImage(&targetImage);

    ComPtr<ID2D1Bitmap1> targetBitmap;

    if (targetImage)
    {
        HRESULT hr = targetImage.As(&targetBitmap);

        if (FAILED(hr))
        {
            // The image brush's image isn't a bitmap, so we can't switch to bitmap brush.
            return;
        }
    }

    SwitchToBitmapBrush(targetBitmap.Get());
}

ActivatableClassWithFactory(CanvasImageBrush, CanvasImageBrushFactory);
